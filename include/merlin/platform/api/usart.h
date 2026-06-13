// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_PLATFORM_API_USART_H
#define MERLIN_PLATFORM_API_USART_H

/**
 * @file usart.h
 *
 * @brief USART bus controller driver unified API definition.
 *
 * This header defines the unified API for USART bus controller drivers in the Merlin platform. It provides a set of
 * function prototypes and data structures that allow applications to interact with USART devices in a consistent manner,
 * regardless of the underlying hardware implementation.
 *
 * USART device drivers developpers are required to comply with this API. They can, if needed, complement
 * additional functions and data structures, but the ones defined in this header must be implemented and behave
 * as specified to help with upper layer portability and interoperability across different platforms and devices.
 *
 * A clean device driver implementation should define its own symbols and alias all of exposed functions to
 * the ones defined in this header, so that the calling application can use the same API regardless of the underlying
 * driver implementation.
 *
 * Note that these functions aim not to be blocking, as the calling application may have an event loop that needs to
 * be responsive. For application that allows blocking calls (e.g. when emitting or receiving a large amount of data),
 * the blocking implementation can be provided as an extension of the non-blocking one,
 * for example by providing a usart_write_blocking() function that internally calls usart_write() in a loop until all
 * data is transmitted, or through an intermediate layer that handles the blocking behavior.
 */

/* usart related types definition*/
#include <merlin/buses/usart.h>
/* include generic status code definition for device drivers */
#include <merlin/platform/api/generic.h>

/**
 * @brief Probe a USART/UART controller using its DTS-declared label.
 *
 * Registers the driver instance into Merlin using the DTS label that
 * uniquely identifies the controller peripheral.  This only performs
 * registration; the device is neither mapped nor initialised at this
 * stage.
 *
 * @param label sentry,label DTS attribute value as declared in the DTS node
 *
 * @return DRV_STATUS_OK on success
 * @return DRV_ERROR_NOTREGISTERED if the merlin registration step failed
 * @return DRV_ERROR_INVSTATE if the driver instance is already probed
 * @return DRV_ERROR_CONFIGURATION if the device is not found in the DTS
 */
drv_status_t usart_probe(uint32_t label);

/**
 * @brief Initialise a USART/UART controller.
 *
 * Maps the device into the caller address space, configures the associated
 * GPIOs (TX/RX and optional CTS/RTS pins) and programs the line parameters.
 * Must be called after a successful usart_probe() call.
 *
 * @param label DTS label identifying the controller instance
 * @param cfg   line configuration (baudrate, parity, stop bits, …)
 *
 * @return DRV_STATUS_OK on success
 * @return DRV_ERROR_NOTREGISTERED was not previously registered
 * @return DRV_ERROR_INVSTATE if the driver instance is already initialised
 * @return DRV_ERROR_CONFIGURATION if the initialization step failed (e.g. pinmux, clock, and so on)
 * @return DRV_ERROR_INVPARAM if the provided parameters are invalid
 */
drv_status_t usart_init(uint32_t label, const struct usart_config *cfg);

/**
 * @brief Transmit a byte over a USART/UART TX path.
 *
 * Upper layer driver may implement a higher level function that transmits a buffer
 * of bytes, but this function is the lowest level function that transmits a single
 * byte over the TX path. This API is requested.
 *
 * Not that this function should, most of the time, not be a blocking call, as
 * the calling application may have an event loop that needs to be responsive.
 *
 * @param label DTS label identifying the controller instance
 * @param wrbuf bytes to transmit
 * @param len   number of bytes to transmit
 *
 * @return DRV_STATUS_OK on success
 * @return DRV_ERROR_INVSTATE if the driver instance is not initialised
 * @return DRV_ERROR_INVPARAM if the provided parameters are invalid
 * @return DRV_ERROR_AGAIN if the transmission failed (e.g. previous
 *    transmission still in progress), but can be retried later
 */
drv_status_t usart_write(uint32_t label, const uint8_t data);

/**
 * @brief Receive bytes from a USART/UART RX path.
 *
 * polling mode or interrupt mode is under the device driver implementation
 * responsability.
 *
 * @param label DTS label identifying the controller instance
 * @param rdbuf buffer used to store received bytes
 * @param len   number of bytes to receive
 *
 * @return DRV_STATUS_OK on success
 * @return DRV_ERROR_INVSTATE if the driver instance is not initialised
 * @return DRV_ERROR_INVPARAM if the provided parameters are invalid
 * @return DRV_ERROR_AGAIN no byte received, but the operation can be retried later
 */
drv_status_t usart_read(uint32_t label, uint8_t *data);

/**
 * @brief Wait for all pending TX data to be shifted out.
 *
 * Polls the Transmission Complete flag until it is set or the retry
 * counter expires.
 *
 * @param label DTS label identifying the controller instance
 *
 * @return DRV_STATUS_OK on success
 * @return DRV_ERROR_INVSTATE if the driver instance is not initialised
 * @return DRV_ERROR_TIMEOUT if the controller did not report TX complete in time
 */
drv_status_t usart_flush(uint32_t label);

/**
 * @brief Release a USART/UART controller.
 *
 * Disables the controller, unmaps the device from the caller address
 * space and frees the instance slot so it can be re-probed.
 *
 * @param label DTS label identifying the controller instance
 *
 * @return DRV_STATUS_OK on success
 * @return DRV_ERROR_INVSTATE if the driver instance is not initialised
 * @return DRV_ERROR_CONFIGURATION on unmap/teardown failure
 */
drv_status_t usart_release(uint32_t label);


#endif /* MERLIN_PLATFORM_API_USART_H */
