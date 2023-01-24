#include <stddef.h>

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/dwt.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>

// Useful documentation and examples:
// http://libopencm3.org/docs/latest/stm32f1/html/group__peripheral__apis.html
// https://github.com/libopencm3/libopencm3-examples/blob/master/examples/stm32/f1/obldc/systick/systick.c
// https://github.com/libopencm3/libopencm3-examples/blob/master/examples/stm32/f1/obldc/usart_irq/usart_irq.c
// https://www.st.com/resource/en/datasheet/stm32f103c8.pdf
// https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf


void busywait(uint32_t cycles);
void dmx_start_transmitting(void);
void handle_dmx(void);


int main(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_USART3);

	// Enable cycle counter to use for some timing
	dwt_enable_cycle_counter();

	// LED pin
	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
	// DMX RX pin
	gpio_set(GPIOB, GPIO11);
	gpio_set_mode(GPIOB, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, GPIO11);
	// DMX TX pin
	gpio_set(GPIOB, GPIO10);
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO10);
	// DMX TX enable pin
	gpio_clear(GPIOB, GPIO14);
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO14);

	usart_set_baudrate(USART3, 250000);
	usart_set_databits(USART3, 8);
	usart_set_stopbits(USART3, USART_STOPBITS_2);
	usart_set_parity(USART3, USART_PARITY_NONE);
	usart_set_flow_control(USART3, USART_FLOWCONTROL_NONE);
	usart_set_mode(USART3, USART_MODE_TX_RX);
	usart_disable_tx_interrupt(USART3);
	usart_enable_rx_interrupt(USART3);
	// There seems to be some errors other than the framing errors from break conditions,
	// so disable error interrupt.
	// Looks like RX interrupt happens from the break anyway.
	// Does this actually work...?
	//usart_enable_error_interrupt(USART3);
	usart_enable(USART3);

	systick_set_reload(1000000);
	systick_interrupt_enable();
	systick_counter_enable();
	nvic_enable_irq(NVIC_USART3_IRQ);

	for (;;) {
		handle_dmx();
	};
}


// Simple busy-waiting delay function
void busywait(uint32_t cycles)
{
	uint32_t start = dwt_read_cycle_counter();
	while (dwt_read_cycle_counter() - start < cycles);
}


void sys_tick_handler(void)
{
	gpio_toggle(GPIOC, GPIO13);
}



// Buffer for received data, only used for testing now
struct rxdata {
	// Framing error happened (probably break condition)
	uint8_t fe;
	// Data was available
	uint8_t rxne;
	// TX buffer empty
	uint8_t txe;
	// Data byte
	uint8_t data;
};
#define RXBUF_LEN 1000
volatile struct rxdata rxbuf[RXBUF_LEN];
volatile size_t rxbuf_i = 0;

#define RX_PACKET_LEN 7
volatile size_t rx_packet_position = 0; // Where reader is during DMX-packet reading
volatile uint8_t rx_packet[RX_PACKET_LEN];
volatile bool rx_packet_received = false;

#define TX_PACKET_LEN 7
volatile size_t tx_packet_position = 0; // Where transmitter is during DMX-packet transmission
volatile uint8_t tx_packet[TX_PACKET_LEN];
volatile bool tx_packet_available = false;


void usart3_isr(void)
{
	// Read all the flags only once in the beginning, so we don't get
	// weird surprises if they happen to change during the ISR.

	// Framing error flag, indicates break condition
	bool flag_fe = usart_get_flag(USART3, USART_SR_FE);
	// "Received data available" flag
	bool flag_rxne = usart_get_flag(USART3, USART_SR_RXNE);
	// "Next byte can be transmitted" flag
	bool flag_txe = usart_get_flag(USART3, USART_SR_TXE);
	// Received data
	uint16_t data = 0;
	if (flag_rxne) {
		data = usart_recv(USART3);
	}

	/* DEBUG START */
	rxbuf[rxbuf_i++] = (struct rxdata) {
		.fe = flag_fe,
		.rxne = flag_rxne,
		.txe = flag_txe,
		.data = data,
	};
	if (rxbuf_i >= RXBUF_LEN)
		rxbuf_i = 0;
	/* DEBUG END*/

	/* PACKET RECEPTION BEGIN */
	size_t rx_packet_position_un_volatiled = rx_packet_position;
	if (flag_fe){
		rx_packet_position_un_volatiled = 0;
	} else if (flag_rxne) {
		if( rx_packet_position_un_volatiled < RX_PACKET_LEN) {
			rx_packet[rx_packet_position_un_volatiled] = data;
			rx_packet_position_un_volatiled += 1; // increment the 
			if (rx_packet_position_un_volatiled == RX_PACKET_LEN){
				rx_packet_received = true;
			} 
		}
	}

	rx_packet_position = rx_packet_position_un_volatiled;
	/* PACKET RECEPTION END */

	/* PACKET TRANSMISSION BEGIN */
	bool tx_packet_available_un_volatiled = tx_packet_available;
	size_t tx_packet_position_un_volatiled = tx_packet_position;
	if (flag_txe) {
		if (tx_packet_available_un_volatiled && tx_packet_position_un_volatiled < TX_PACKET_LEN) {
			usart_send(USART3, tx_packet[tx_packet_position_un_volatiled]);
			tx_packet_available_un_volatiled += 1;
			if (tx_packet_position_un_volatiled >= TX_PACKET_LEN) {
				// Transmission ended.
				tx_packet_available_un_volatiled = false;
				usart_disable_tx_interrupt(USART3);
			}
		} else {
			// Nothing to transmit.
			// Make sure TX interrupt is disabled.
			usart_disable_tx_interrupt(USART3);
		}
	}
	tx_packet_available = tx_packet_available_un_volatiled;
	tx_packet_position = tx_packet_position_un_volatiled;
	/* PACKET TRANSMISSION END */
}


// Call this when tx_packet has data ready to transmit.
void dmx_start_transmitting(void)
{
	// Should not be necessary here, but let's try...
	usart_disable_tx_interrupt(USART3);

	// Send break condition by changing the TX pin to a normal GPIO output.
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO10);
	busywait(10000); // make sure there's some idle in between, not sure if necessary
	gpio_clear(GPIOB, GPIO10);
	busywait(10000);
	// Mark after break
	gpio_set(GPIOB, GPIO10);
	busywait(1000);
	// Then change it back to UART mode
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO10);

	// Tell the interrupt handler it can start transmitting
	tx_packet_position = 0;
	tx_packet_available = 1;
	usart_enable_tx_interrupt(USART3);
}


// Called from main loop to do DMX processing.
void handle_dmx(void)
{
	if (rx_packet_received && !tx_packet_available) {
		// Slot 0 is always 0
		tx_packet[0] = 0;

		// Channel 1 does stupid things in the current setup, so keep it at 0
		tx_packet[1] = 0;

		// Invert the current RGB channels to see it's working
		for (size_t i = 2; i <= 4; i++) {
			tx_packet[i] = 255 - rx_packet[i];
		}

		// Set the rest of channels to 0
		for (size_t i = 5; i < TX_PACKET_LEN; i++) {
			tx_packet[i] = 0;
		}

		// Mark the receive buffer free when we are done reading it
		rx_packet_received = 0;

		dmx_start_transmitting();
	}
}
