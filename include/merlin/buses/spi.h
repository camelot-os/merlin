// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_SPI_H
#define MERLIN_SPI_H

#include <types.h>
#include <merlin/platform/api/generic.h>
#include <merlin/platform/driver.h>

/**
 * @brief Generic SPI controller role.
 */
enum spi_controller_mode {
	SPI_CONTROLLER_MODE_MASTER, /**< Master mode */
	SPI_CONTROLLER_MODE_SLAVE,  /**< Slave mode */
	SPI_CONTROLLER_MODE_LOOPBACK, /**< Loopback mode (for testing) */
};

/**
 * @brief SPI transfer topology.
 */
enum spi_duplex_mode {
	SPI_DUPLEX_FULL,		/**< Full duplex exchanges */
	SPI_DUPLEX_HALF,		/**< Half duplex exchanges */
	SPI_DUPLEX_SIMPLEX_TX,	/**< Simplex transmit only */
	SPI_DUPLEX_SIMPLEX_RX,	/**< Simplex receive only */
};

/**
 * @brief SPI frame format.
 */
enum spi_frame_format {
	SPI_FRAME_FORMAT_MOTOROLA, /**< Standard SPI frame format */
	SPI_FRAME_FORMAT_TI,       /**< TI SPI frame format */
};

/**
 * @brief SPI clock phase (CPHA).
 */
enum spi_clock_phase {
	SPI_CPHA_CAPTURE_FIRST,   /**< Capture on the first clock edge */
	SPI_CPHA_CAPTURE_SECOND,  /**< Capture on the second clock edge */
};

/**
 * @brief SPI clock polarity (CPOL).
 */
enum spi_clock_polarity {
	SPI_CPOL_LOW,			/**< Clock idle state is low */
	SPI_CPOL_HIGH,			/**< Clock idle state is high */
};

/**
 * @brief SPI bit transmission order.
 */
enum spi_bit_order {
	SPI_BIT_ORDER_MSB_FIRST, /**< Most significant bit first */
	SPI_BIT_ORDER_LSB_FIRST, /**< Least significant bit first */
};

/**
 * @brief SPI chip-select handling policy.
 */
enum spi_cs_management {
	SPI_CS_HARDWARE,
	SPI_CS_SOFTWARE,
	SPI_CS_MANAGEMENT_UNKNOWN,
};

/**
 * @brief SPI active level for the chip-select line.
 */
enum spi_cs_polarity {
	SPI_CS_ACTIVE_LOW,	/**< Chip-select active low */
	SPI_CS_ACTIVE_HIGH,	/**< Chip-select active high */
};

enum spi_speed {
	SPI_SPEED_LOW, 		/**< Low speed */
	SPI_SPEED_MEDIUM, 	/**< Medium speed */
	SPI_SPEED_HIGH, 	/**< High speed */
};

/**
 * @brief Canonical configuration for a standard SPI controller.
 *
 * speed_hz and word_size_bits are the two key parameters used by most SPI
 * controller IPs. word_size_bits is usually within [4..32], but the effective
 * supported range remains controller-specific.
 *
 * This configuration is used as input for the spi_init() function, and is expected
 * to be filled by driver calling layer, based on the previous information set.
 */
struct spi_config {
	uint32_t speed_hz;
	uint8_t word_size_bits;
	enum spi_controller_mode controller_mode;
	enum spi_duplex_mode duplex_mode;
	enum spi_frame_format frame_format;
	enum spi_clock_phase cpha;
	enum spi_clock_polarity cpol;
	enum spi_bit_order bit_order;
	enum spi_cs_management cs_management;
	enum spi_cs_polarity cs_polarity;
	uint8_t cs_index;
	bool cs_keep_active;
};

drv_status_t spi_probe(uint32_t label);
drv_status_t spi_init(uint32_t label, struct spi_config *config);
drv_status_t spi_xfer(uint32_t label, uint8_t *rdbuf, const uint8_t *wrbuf, size_t len);
drv_status_t spi_release(uint32_t label);
drv_status_t spi_set_cs(uint32_t label, uint8_t cs_index, bool active);



#endif/*!MERLIN_SPI_H*/
