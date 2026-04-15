// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_ARCH_USART_H
#define MERLIN_ARCH_USART_H

#include <types.h>
#include <merlin/platform/driver.h>

/**
 * @brief Serial controller operating modes supported by the unified UART/USART API.
 *
 * A pure UART controller typically supports only asynchronous mode, while a USART
 * controller can additionally expose synchronous transfers. This enum provides a
 * single abstraction to describe both controller families.
 */
enum usart_operation_mode {
	USART_MODE_ASYNCHRONOUS,
	USART_MODE_SYNCHRONOUS,
	USART_MODE_UNKNOWN,
};

/**
 * @brief Serial parity modes.
 */
enum usart_parity {
	USART_PARITY_NONE,
	USART_PARITY_EVEN,
	USART_PARITY_ODD,
	USART_PARITY_UNKNOWN,
};

/**
 * @brief Serial stop-bit configurations.
 */
enum usart_stop_bits {
	USART_STOP_BITS_1,
	USART_STOP_BITS_0_5,
	USART_STOP_BITS_2,
	USART_STOP_BITS_1_5,
	USART_STOP_BITS_UNKNOWN,
};

/**
 * @brief Serial word-length configurations.
 */
enum usart_word_length {
	USART_WORD_LENGTH_7,
	USART_WORD_LENGTH_8,
	USART_WORD_LENGTH_9,
	USART_WORD_LENGTH_UNKNOWN,
};

/**
 * @brief Hardware flow-control modes.
 */
enum usart_flow_control {
	USART_FLOW_CONTROL_NONE,
	USART_FLOW_CONTROL_RTS,
	USART_FLOW_CONTROL_CTS,
	USART_FLOW_CONTROL_RTS_CTS,
	USART_FLOW_CONTROL_UNKNOWN,
};

/**
 * @brief Unified line configuration for UART and USART controllers.
 *
 * The backend DTS integration can later use this structure to expose the
 * controller capabilities and default parameters in a common way.
 *
 */
struct usart_config {
	uint32_t baudrate;
	enum usart_operation_mode mode;
	enum usart_parity parity;
	enum usart_stop_bits stop_bits;
	enum usart_word_length word_length;
	enum usart_flow_control flow_control;
	bool tx_enable;
	bool rx_enable;
};

/** USART driver structure pre-declaration */
struct usart_driver;

/**
 * @brief Configure the serial controller line parameters.
 *
 * @param self pointer to the usart_driver structure
 * @param config unified UART/USART line configuration
 *
 * @return 0 on success, negative error code on failure
 */
typedef int (*usart_configure_fn_t)(struct usart_driver *self, const struct usart_config *config);

/**
 * @brief Write bytes to the controller transmit path.
 *
 * @param self pointer to the usart_driver structure
 * @param wrbuf buffer containing bytes to transmit
 * @param len number of bytes to transmit
 *
 * @return 0 on success, negative error code on failure
 */
typedef int (*usart_write_fn_t)(struct usart_driver *self, const uint8_t *wrbuf, size_t len);

/**
 * @brief Read bytes from the controller receive path.
 *
 * @param self pointer to the usart_driver structure
 * @param rdbuf buffer used to store received bytes
 * @param len maximum number of bytes to receive
 *
 * @return 0 on success, negative error code on failure
 */
typedef int (*usart_read_fn_t)(struct usart_driver *self, uint8_t *rdbuf, size_t len);

/**
 * @brief Flush pending transmit data if the controller provides such an operation.
 *
 * @param self pointer to the usart_driver structure
 *
 * @return 0 on success, negative error code on failure
 */
typedef int (*usart_flush_fn_t)(struct usart_driver *self);

/**
 * @brief USART/UART specific operations exposed to Merlin.
 *
 * These callbacks describe the minimum common contract expected from a serial
 * controller driver registered in Merlin.
 */
struct usart_bus_fops {
	usart_configure_fn_t configure;
	usart_write_fn_t write;
	usart_read_fn_t read;
	usart_flush_fn_t flush;
};

/**
 * @brief Unified UART/USART controller descriptor registered in Merlin.
 *
 * This structure does not implement the driver logic itself. It only binds the
 * serial-specific operations to the generic Merlin platform registration layer.
 */
struct usart_driver {
	struct usart_bus_fops *fops;
	struct platform_device_driver platform;
	void *private_data;
};

#endif/*!MERLIN_ARCH_USART_H*/
