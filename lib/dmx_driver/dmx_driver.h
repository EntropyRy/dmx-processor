// DMX driver for STM32F1 series using libopencm3
//
//   Thread safety/reentrancy/concurrency considerations:
//   ---------------------------------------------------
// Thread safety between calls to (most) dmxdrv functions is not guaranteed.
// Recommended approach is to call them only from your main loop
// (or more generally, from only one task in case of a RTOS, or from only
// one interrupt priority level in case of fully interrupt-driven code).
// For example, calling dmxdrv_tx_buffer_available while dmxdrv_start_tx
// is executing in another context may not return meaningful results.
//
// An exception is, of course, the dmxdrv_usart_isr function, which is
// meant to be called from a high priority interrupt. It can safely happen
// at any moment, as long as the driver instance has been initialized.

#ifndef DMX_DRIVER_H
#define DMX_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Maximum length of a received packet.
// This determines the size of receive buffers.
#define DMXDRV_RX_MAX_LEN 7
// Maximum length of a transmitted packet.
// This determines the size of transmit buffers.
#define DMXDRV_TX_MAX_LEN 512

// Settings for a DMX driver instance
struct dmxdrv_settings {
	// USART peripheral identifier
	uint32_t usart;
	// GPIO port identifier of receive pin
	uint32_t rx_port;
	// GPIO port identifier of transmit pin
	uint32_t tx_port;
	// GPIO port identifier of driver enable pin
	uint32_t de_port;
	// GPIO pin identifier of receive pin
	uint16_t rx_pin;
	// GPIO pin identifier of transmit pin
	uint16_t tx_pin;
	// GPIO pin identifier of driver enable pin
	uint16_t de_pin;
};

struct dmxdrv;
typedef struct dmxdrv *dmxdrv_t;

// Initialize a DMX driver instance with the given settings.
//
// Clocks of the given GPIO and UART peripherals should be enabled
// before calling dmxdrv_init.
//
// USART IRQ should be enabled using nvic_enable_irq after calling
// dmxdrv_init and when it is safe to enable interrupts.
// This makes sure the ISR will not get called before dmxdrv_init
// has finished and the return value has been stored.
dmxdrv_t dmxdrv_init(const struct dmxdrv_settings *settings);

// Handle interrupts of the USART peripheral used by the DMX driver.
//
// Call this from the USART ISR.
//
// Note that this runs in an interrupt context, and call to this
// function may happen during execution of other dmxdrv functions.
void dmxdrv_usart_isr(dmxdrv_t dmxdrv);

// Check if a new received packet is available.
//
// If this function returns true, the packet can be read using
// dmxdrv_get_rx_packet and dmxdrv_get_rx_length.
// After processing the data, the packet should be freed using
// dmxdrv_free_rx_packet.
bool dmxdrv_rx_packet_available(dmxdrv_t dmxdrv);

// Return a pointer to the data of the next available packet.
//
// This function should only be called after dmxdrv_rx_packet_available
// has returned true.
const uint8_t *dmxdrv_get_rx_packet(dmxdrv_t dmxdrv);

// Return the length of the data of the next available packet.
//
// This function should only be called after dmxdrv_rx_packet_available
// has returned true.
size_t dmxdrv_get_rx_length(dmxdrv_t dmxdrv);

// Mark the packet buffer previously returned by dmxdrv_get_rx_packet
// as free. Call this when the data is no longer needed.
//
// This tells the driver it can use the buffer again
// to receive the next packet.
void dmxdrv_free_rx_packet(dmxdrv_t dmxdrv);

// TODO
bool dmxdrv_tx_buffer_available(dmxdrv_t dmxdrv);

// TODO
uint8_t *dmxdrv_get_tx_buffer(dmxdrv_t dmxdrv);

// TODO
void dmxdrv_set_tx_length(dmxdrv_t dmxdrv, size_t length);

// TODO
void dmxdrv_start_tx(dmxdrv_t dmxdrv);


#endif
