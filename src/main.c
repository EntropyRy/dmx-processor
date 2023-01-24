#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>

// Useful documentation and examples:
// http://libopencm3.org/docs/latest/stm32f1/html/group__peripheral__apis.html
// https://github.com/libopencm3/libopencm3-examples/blob/master/examples/stm32/f1/obldc/systick/systick.c
// https://github.com/libopencm3/libopencm3-examples/blob/master/examples/stm32/f1/obldc/usart_irq/usart_irq.c
// https://www.st.com/resource/en/datasheet/stm32f103c8.pdf
// https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf



int main(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_USART3);

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
	usart_enable_rx_interrupt(USART3);
	usart_enable_error_interrupt(USART3);
	usart_enable(USART3);

	systick_set_reload(1000000);
	systick_interrupt_enable();
	systick_counter_enable();
	nvic_enable_irq(NVIC_USART3_IRQ);

	for (;;) {



	};
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
	// Data byte
	uint8_t data;
};
#define RXBUF_LEN 1000
volatile struct rxdata rxbuf[RXBUF_LEN];
volatile int rxbuf_i = 0;

#define RX_PACKET_LEN 7
volatile int rx_packet_position = 0; // Where reader is during DMX-packet reading
volatile uint8_t rx_packet[RX_PACKET_LEN];
volatile bool DMX_packet_recieved = false;

void usart3_isr(void)
{

	/* DEBUG START */
	bool flag_fe = usart_get_flag(USART3, USART_SR_FE);
	short flag_rxne = usart_get_flag(USART3, USART_SR_RXNE);
	short data = 0;
	if (flag_rxne) {
		data = usart_recv(USART3);
	}
	rxbuf[rxbuf_i++] = (struct rxdata) {
		.fe = flag_fe,
		.rxne = flag_rxne,
		.data = data,
	};
	if (rxbuf_i >= RXBUF_LEN)
		rxbuf_i = 0;
	/* DEBUG END*/
	int rx_packet_position_un_volatiled = rx_packet_position;
	if (flag_fe){
		rx_packet_position_un_volatiled = 0;
	} else {
		if( rx_packet_position_un_volatiled < RX_PACKET_LEN) {
			rx_packet[rx_packet_position_un_volatiled] = data;
			rx_packet_position_un_volatiled += 1; // increment the 
			if (rx_packet_position_un_volatiled == RX_PACKET_LEN){
				DMX_packet_recieved = true;
			} 
		}
	}

	rx_packet_position = rx_packet_position_un_volatiled;
}
