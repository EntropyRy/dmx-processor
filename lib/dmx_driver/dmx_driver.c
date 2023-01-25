#include "dmx_driver.h"

#include <stdlib.h>

#include <libopencm3/cm3/dwt.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>


struct dmxdrv {
	// Position where the receiver is going
	volatile size_t rx_position;

	// Add when interrupt-driven TX is implemented
	//volatile size_t tx_position;

	// rx_packet_in_buffer is used to control access to rx_packet
	// between interrupts and main loop.
	// If rx_packet_in_buffer is false, ISR is allowed to write
	// to rx_packet and main loop should not touch it.
	// When packet reception is complete, ISR changes
	// rx_packet_in_buffer to true and does not touch rx_packet
	// anymore. This tells the main loop it can start processing
	// the received packet. When main loop is done with the packet,
	// it sets rx_packet_in_buffer back to false, telling the
	// ISR it can use it for the next packet.
	bool rx_packet_in_buffer;

	// Add when interrupt-driven TX is implemented
	//bool tx_packet_in_buffer;

	struct dmxdrv_settings settings;

	uint8_t rx_packet[DMXDRV_RX_MAX_LEN];
	uint8_t tx_packet[DMXDRV_TX_MAX_LEN];
};


dmxdrv_t dmxdrv_init(const struct dmxdrv_settings *settings)
{
	struct dmxdrv *dmxdrv;
	dmxdrv = calloc(1, sizeof(*dmxdrv));

	dmxdrv->settings = *settings;

	// Enable cycle counter to use for some timing
	dwt_enable_cycle_counter();

	// DMX RX pin
	gpio_set(
			dmxdrv->settings.rx_port,
			dmxdrv->settings.rx_pin);
	gpio_set_mode(
			dmxdrv->settings.rx_port,
			GPIO_MODE_INPUT,
			GPIO_CNF_INPUT_PULL_UPDOWN,
			dmxdrv->settings.rx_pin);
	// DMX TX pin
	gpio_set(
		dmxdrv->settings.tx_port,
		dmxdrv->settings.tx_pin);
	gpio_set_mode(
		dmxdrv->settings.tx_port,
		GPIO_MODE_OUTPUT_2_MHZ,
		GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,
		dmxdrv->settings.tx_pin);
	// DMX TX enable pin
	gpio_clear(
		dmxdrv->settings.de_port,
		dmxdrv->settings.de_pin);
	gpio_set_mode(
		dmxdrv->settings.de_port,
		GPIO_MODE_OUTPUT_2_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL,
		dmxdrv->settings.de_pin);

	const uint32_t usart = dmxdrv->settings.usart;
	usart_set_baudrate(usart, 250000);
	usart_set_databits(usart, 8);
	usart_set_stopbits(usart, USART_STOPBITS_2);
	usart_set_parity(usart, USART_PARITY_NONE);
	usart_set_flow_control(usart, USART_FLOWCONTROL_NONE);
	usart_set_mode(usart, USART_MODE_TX_RX);
	usart_enable_rx_interrupt(usart);
	usart_enable(usart);

	return dmxdrv;
}

void dmxdrv_usart_isr(dmxdrv_t dmxdrv)
{
	const uint32_t usart = dmxdrv->settings.usart;

	// Read all the flags only once in the beginning, so we don't get
	// weird surprises if they happen to change during the ISR.

	// Framing error flag, indicates break condition
	bool flag_fe = usart_get_flag(usart, USART_SR_FE);
	// "Received data available" flag
	bool flag_rxne = usart_get_flag(usart, USART_SR_RXNE);
	// "Next byte can be transmitted" flag
	//bool flag_txe = usart_get_flag(usart, USART_SR_TXE);
	// Received data
	uint16_t data = 0;
	if (flag_rxne) {
		data = usart_recv(usart);
	}

	// Packet reception

	// Copy position to local variable
	// to make sure nothing changes it in between.
	size_t rx_position = dmxdrv->rx_position;

	if (flag_fe) {
		// TODO: check that rx_packet_in_buffer is false
		// and abort packet reception otherwise.
		rx_position = 0;
	} else if (flag_rxne) {
		if (rx_position < DMXDRV_RX_MAX_LEN) {
			dmxdrv->rx_packet[rx_position] = data;
			rx_position += 1;
			// TODO: support configuring a length shorter than the maximum length?
			if (rx_position == DMXDRV_RX_MAX_LEN){
				dmxdrv->rx_packet_in_buffer = true;
			}
		}
	}

	dmxdrv->rx_position = rx_position;
}

bool dmxdrv_rx_packet_available(dmxdrv_t dmxdrv)
{
	return dmxdrv->rx_packet_in_buffer;
}

const uint8_t *dmxdrv_get_rx_packet(dmxdrv_t dmxdrv)
{
	// This would not really have to be a function at all,
	// since the caller could as well directly access
	// the rx_packet member of the struct.
	// Making it a function, however, allows keeping the API the same
	// even if something like double buffering (or a longer FIFO)
	// of packets is implemented in future.
	// Maybe this is a somewhat cleaner approach too.
	return dmxdrv->rx_packet;
}

size_t dmxdrv_get_rx_length(dmxdrv_t dmxdrv)
{
	// TODO: support packets of a different length?
	(void)dmxdrv; //unused
	return DMXDRV_RX_MAX_LEN;
}

void dmxdrv_free_rx_packet(dmxdrv_t dmxdrv)
{
	dmxdrv->rx_packet_in_buffer = false;
}

bool dmxdrv_tx_buffer_available(dmxdrv_t dmxdrv)
{
	(void)dmxdrv; //unused
	return true;
}

uint8_t *dmxdrv_get_tx_buffer(dmxdrv_t dmxdrv)
{
	return dmxdrv->tx_packet;
}

void dmxdrv_set_tx_length(dmxdrv_t dmxdrv, size_t length)
{
	// TODO
	(void)dmxdrv; //unused
	(void)length; //unused
}

// Simple busy-waiting delay function
static void busywait(uint32_t cycles)
{
	uint32_t start = dwt_read_cycle_counter();
	while (dwt_read_cycle_counter() - start < cycles);
}

void dmxdrv_start_tx(dmxdrv_t dmxdrv)
{
	// Send break condition by changing the TX pin to a normal GPIO output.
	gpio_set_mode(
		dmxdrv->settings.tx_port,
		GPIO_MODE_OUTPUT_2_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL,
		dmxdrv->settings.tx_pin);
	busywait(10000); // make sure there's some idle in between, not sure if necessary
	gpio_clear(dmxdrv->settings.tx_port, dmxdrv->settings.tx_pin);
	busywait(10000);
	// Mark after break
	gpio_set(dmxdrv->settings.tx_port, dmxdrv->settings.tx_pin);
	busywait(1000);
	// Then change it back to UART mode
	gpio_set_mode(
		dmxdrv->settings.tx_port,
		GPIO_MODE_OUTPUT_2_MHZ,
		GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,
		dmxdrv->settings.tx_pin);

	// Ok, let's forget about interrupt based transmissions for now.
	// Let's be stupidly inefficient and busy-wait the whole transmission here.
	for (int i = 0; i < DMXDRV_TX_MAX_LEN; i++) {
		usart_send_blocking(dmxdrv->settings.usart, dmxdrv->tx_packet[i]);
	}
}
