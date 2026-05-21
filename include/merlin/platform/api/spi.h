// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_PLATFORM_API_SPI_H
#define MERLIN_PLATFORM_API_SPI_H

#include <inttypes.h>
#include <stddef.h>
#include <merlin/platform/api/generic.h>

/* SPI bus configuration and speed definitions are a part of upper layer exported infos */
#include <merlin/buses/spi.h>


drv_status_t spi_probe(uint32_t label);

/**
 * @brief Configure SPI controller parameters.
 *
 * @param self pointer to the spi_driver structure
 * @param config SPI bus and frame configuration
 *
 * @return 0 on success, negative error code on failure
 */
drv_status_t spi_init(uint32_t label, struct spi_config *config);

/**
 * @brief Transfer bytes over SPI.
 *
 * wrbuf and rdbuf may be NULL depending on transfer direction and controller
 * capabilities. len is expressed in bytes.
 *
 * @param label SPI bus label
 * @param rdbuf buffer receiving read data, or NULL
 * @param wrbuf buffer containing data to send, or NULL
 * @param len number of bytes to transfer
 *
 * @return 0 on success, negative error code on failure
 */
drv_status_t spi_xfer(uint32_t label, uint8_t *rdbuf, const uint8_t *wrbuf, size_t len);

/**
 * @brief Optional software chip-select control.
 *
 * @param label SPI bus label
 * @param cs_index chip-select index to drive
 * @param active true to assert CS, false to deassert it
 *
 * @return 0 on success, negative error code on failure
 */
drv_status_t spi_set_cs(uint32_t label, uint8_t cs_index, bool active);


#endif /* MERLIN_PLATFORM_API_SPI_H */
