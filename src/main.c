#include <stddef.h>

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>

#include "dmx_driver.h"

// Useful documentation and examples:
// http://libopencm3.org/docs/latest/stm32f1/html/group__peripheral__apis.html
// https://github.com/libopencm3/libopencm3-examples/blob/master/examples/stm32/f1/obldc/systick/systick.c
// https://github.com/libopencm3/libopencm3-examples/blob/master/examples/stm32/f1/obldc/usart_irq/usart_irq.c
// https://www.st.com/resource/en/datasheet/stm32f103c8.pdf
// https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf


void dmx_main(dmxdrv_t dmx);

dmxdrv_t dmx1;

int main(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_USART3);

	// LED pin
	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);

	dmx1 = dmxdrv_init(&(const struct dmxdrv_settings) {
		.usart = USART3,
		.rx_port = GPIOB,
		.tx_port = GPIOB,
		.de_port = GPIOB,
		.rx_pin = GPIO11,
		.tx_pin = GPIO10,
		.de_pin = GPIO14,
	});

	systick_set_reload(1000000);
	systick_interrupt_enable();
	systick_counter_enable();
	nvic_enable_irq(NVIC_USART3_IRQ);

	for (;;) {
		dmx_main(dmx1);
	}
}


void sys_tick_handler(void)
{
	gpio_toggle(GPIOC, GPIO13);
}


void usart3_isr(void)
{
	dmxdrv_usart_isr(dmx1);
}


// Called from main loop to do DMX processing.
void dmx_main(dmxdrv_t dmx)
{
	if (dmxdrv_rx_packet_available(dmx) && dmxdrv_tx_buffer_available(dmx)) {
		const uint8_t *rx_packet = dmxdrv_get_rx_packet(dmx);
		const size_t rx_length = dmxdrv_get_rx_length(dmx);
		uint8_t *tx_packet = dmxdrv_get_tx_buffer(dmx);
		size_t tx_length = DMXDRV_TX_MAX_LEN;

		// Slot 0 is always 0
		tx_packet[0] = 0;

		// Channel 1 does stupid things in the current setup, so keep it at 0
		tx_packet[1] = 0;

		// Invert the current RGB channels to see it's working
		for (size_t i = 2; i <= 4; i++) {
			tx_packet[i] = 255 - rx_packet[i];
		}

		// Set the rest of channels to 0
		for (size_t i = 5; i < tx_length; i++) {
			tx_packet[i] = 0;
		}

		// Mark the receive buffer free when we are done reading it
		dmxdrv_free_rx_packet(dmx);

		dmxdrv_set_tx_length(dmx, tx_length);
		dmxdrv_start_tx(dmx);
	}
}
