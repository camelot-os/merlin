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
 * @brief Unified UART/USART controller descriptor registered in Merlin.
 *
 * Function prototypes are defined in merlin/platform/api/usart.h.
 * This structure only stores per-instance runtime state for driver internals.
 */
struct usart_driver {
	struct platform_device_driver platform;
	void *private_data;
};

#endif/*!MERLIN_ARCH_USART_H*/
