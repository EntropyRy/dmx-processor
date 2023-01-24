#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

// Useful documentation and examples:
// http://libopencm3.org/docs/latest/stm32f1/html/group__peripheral__apis.html
// https://github.com/libopencm3/libopencm3-examples/blob/master/examples/stm32/f1/obldc/systick/systick.c


int main(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();
	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
	systick_set_reload(1000000);
	systick_interrupt_enable();
	systick_counter_enable();

	for (;;);
}


void sys_tick_handler(void)
{
	gpio_toggle(GPIOC, GPIO13);
}
