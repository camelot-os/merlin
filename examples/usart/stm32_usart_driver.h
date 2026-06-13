// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 H2Lab Development Team

#ifndef STM32_USART_DRIVER_H
#define STM32_USART_DRIVER_H

#include <inttypes.h>
#include <merlin/buses/usart.h>

/**
 * Maximum number of USART/UART instances this driver can manage
 * simultaneously.  Increase as needed for targets with more serial
 * controllers.
 */
#define STM32_USART_MAX_INSTANCES 4U

/* exported API is defined through unified merlin API */
#include <merlin/platform/api/usart.h>

/* API extension example, that delivers blocking read/write functionality */

drv_status_t usart_write_blocking(uint32_t label, const uint8_t *wrbuf, size_t len);


drv_status_t usart_read_blocking(uint32_t label, uint8_t *rdbuf, size_t len);

#endif /* STM32_USART_DRIVER_H */
