// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 H2Lab Development Team

#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>

#include <merlin/io.h>
#include <merlin/helpers.h>

#include "stm32u5_fdcan_driver.h"

/*
 * STM32U5 FDCAN1 is based on Bosch M_CAN IP.
 * This driver targets a pragmatic subset suitable for Merlin samples:
 * - polling mode TX/RX
 * - classic CAN data payload (8 bytes)
 * - optional internal loopback / listen-only modes
 */

#define FDCAN_CREL_OFFSET        0x00UL
#define FDCAN_ENDN_OFFSET        0x04UL
#define FDCAN_DBTP_OFFSET        0x0CUL
#define FDCAN_TEST_OFFSET        0x10UL
#define FDCAN_CCCR_OFFSET        0x18UL
#define FDCAN_NBTP_OFFSET        0x1CUL
#define FDCAN_TSCC_OFFSET        0x20UL
#define FDCAN_TSCV_OFFSET        0x24UL
#define FDCAN_ECR_OFFSET         0x40UL
#define FDCAN_PSR_OFFSET         0x44UL
#define FDCAN_IR_OFFSET          0x50UL
#define FDCAN_IE_OFFSET          0x54UL
#define FDCAN_GFC_OFFSET         0x80UL
#define FDCAN_SIDFC_OFFSET       0x84UL
#define FDCAN_XIDFC_OFFSET       0x88UL
#define FDCAN_RXESC_OFFSET       0xBCUL
#define FDCAN_TXBC_OFFSET        0xC0UL
#define FDCAN_TXFQS_OFFSET       0xC4UL
#define FDCAN_TXESC_OFFSET       0xC8UL
#define FDCAN_TXBAR_OFFSET       0xCCUL
#define FDCAN_TXBTO_OFFSET       0xD0UL
#define FDCAN_RXF0C_OFFSET       0xA0UL
#define FDCAN_RXF0S_OFFSET       0xA4UL
#define FDCAN_RXF0A_OFFSET       0xA8UL

#define FDCAN_CCCR_INIT          (1UL << 0)
#define FDCAN_CCCR_CCE           (1UL << 1)
#define FDCAN_CCCR_ASM           (1UL << 2)
#define FDCAN_CCCR_CSA           (1UL << 3)
#define FDCAN_CCCR_MON           (1UL << 5)
#define FDCAN_CCCR_TEST          (1UL << 7)
#define FDCAN_CCCR_BRSE          (1UL << 8)
#define FDCAN_CCCR_FDOE          (1UL << 9)

#define FDCAN_TEST_LBCK          (1UL << 4)

#define FDCAN_TXFQS_TFFL_MASK    0x3FUL
#define FDCAN_TXFQS_TFQPI_SHIFT  16U
#define FDCAN_TXFQS_TFQPI_MASK   (0x1FUL << FDCAN_TXFQS_TFQPI_SHIFT)
#define FDCAN_TXFQS_TFQF         (1UL << 21)

#define FDCAN_RXF0S_F0FL_MASK    0x7FUL
#define FDCAN_RXF0S_F0GI_SHIFT   8U
#define FDCAN_RXF0S_F0GI_MASK    (0x3FUL << FDCAN_RXF0S_F0GI_SHIFT)

#define FDCAN_TXBC_NDTB_SHIFT    16U
#define FDCAN_TXBC_TFQS_SHIFT    24U
#define FDCAN_TXBC_TBSA_SHIFT    2U

#define FDCAN_RXF0C_F0SA_SHIFT   2U
#define FDCAN_RXF0C_F0S_SHIFT    16U

#define FDCAN_NBTP_NTSEG2_SHIFT  0U
#define FDCAN_NBTP_NTSEG1_SHIFT  8U
#define FDCAN_NBTP_NBRP_SHIFT    16U
#define FDCAN_NBTP_NSJW_SHIFT    25U

#define FDCAN_IR_ALL             0x3FFFFFFFUL

#define FDCAN_FRAME_XTD          (1UL << 30)
#define FDCAN_FRAME_RTR          (1UL << 29)

#define FDCAN_FRAME_DLC_SHIFT    16U
#define FDCAN_FRAME_BRS          (1UL << 20)
#define FDCAN_FRAME_FDF          (1UL << 21)

#define FDCAN_POLL_RETRIES       100000U

#define STM32U5_FDCAN_MSGRAM_OFFSET   0x800UL
#define FDCAN_RXFIFO0_START_WORD      0U
#define FDCAN_RXFIFO0_ELEM_WORDS      4U
#define FDCAN_RXFIFO0_ELEMS           8U
#define FDCAN_TXFIFO_START_WORD       (FDCAN_RXFIFO0_START_WORD + (FDCAN_RXFIFO0_ELEM_WORDS * FDCAN_RXFIFO0_ELEMS))
#define FDCAN_TXFIFO_ELEM_WORDS       4U
#define FDCAN_TXFIFO_ELEMS            8U

struct stm32_fdcan_ctx {
    bool initialized;
    bool mapped;
    size_t msg_ram_base;
    struct can_config cfg;
};

static struct platform_device_driver g_can_driver = {
    .devh = 0,
    .label = 0,
    .devinfo = NULL,
    .name = "stm32u5 fdcan driver",
    .compatible = "st,stm32-fdcan",
    .platform_fops = {
        .isr = NULL,
    },
    .type = DEVICE_TYPE_CAN,
    .private_data = NULL,
};

static struct stm32_fdcan_ctx g_can_ctx;

static const uint8_t g_can_dlc_to_len[16] = {
    0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U,
    8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U,
};

static inline uint8_t can_len_to_dlc(uint8_t len)
{
    if (len <= 8U) {
        return len;
    }
    if (len <= 12U) {
        return 9U;
    }
    if (len <= 16U) {
        return 10U;
    }
    if (len <= 20U) {
        return 11U;
    }
    if (len <= 24U) {
        return 12U;
    }
    if (len <= 32U) {
        return 13U;
    }
    if (len <= 48U) {
        return 14U;
    }
    return 15U;
}

static inline size_t fdcan_reg(size_t offset)
{
    return ((size_t)g_can_driver.devinfo->baseaddr + offset);
}

static inline uint32_t fdcan_read32(size_t offset)
{
    return merlin_ioread32(fdcan_reg(offset));
}

static inline void fdcan_write32(size_t offset, uint32_t value)
{
    merlin_iowrite32(fdcan_reg(offset), value);
}

static inline size_t fdcan_mram_addr(size_t word_offset)
{
    return g_can_ctx.msg_ram_base + (word_offset * 4U);
}

static int fdcan_enter_init_mode(void)
{
    uint32_t cccr = fdcan_read32(FDCAN_CCCR_OFFSET);

    cccr |= (FDCAN_CCCR_INIT | FDCAN_CCCR_CCE);
    fdcan_write32(FDCAN_CCCR_OFFSET, cccr);

    if (merlin_iopoll32_until_set(fdcan_reg(FDCAN_CCCR_OFFSET),
                                  FDCAN_CCCR_INIT,
                                  FDCAN_POLL_RETRIES) != STATUS_OK) {
        return -1;
    }

    return 0;
}

static int fdcan_leave_init_mode(void)
{
    uint32_t cccr = fdcan_read32(FDCAN_CCCR_OFFSET);

    cccr &= ~(FDCAN_CCCR_INIT | FDCAN_CCCR_CCE);
    fdcan_write32(FDCAN_CCCR_OFFSET, cccr);

    if (merlin_iopoll32_until_clear(fdcan_reg(FDCAN_CCCR_OFFSET),
                                    FDCAN_CCCR_INIT,
                                    FDCAN_POLL_RETRIES) != STATUS_OK) {
        return -1;
    }

    return 0;
}

static int fdcan_apply_nominal_timing(uint32_t bitrate)
{
    uint32_t busfreq_mhz = 0U;
    uint32_t bus_hz;
    uint32_t prescaler;
    uint32_t nbtp;

    if (bitrate == 0U) {
        return -1;
    }

    if (merlin_platform_driver_get_bus_clock(&g_can_driver, &busfreq_mhz) != STATUS_OK ||
        busfreq_mhz == 0U) {
        /* Keep the sample resilient even when the DTS omits can,peripheral-clock. */
        busfreq_mhz = 40U;
    }

    bus_hz = busfreq_mhz * 1000000UL;

    /*
     * Fixed sample point template: Sync(1) + TSEG1(13) + TSEG2(2) = 16 tq.
     * prescaler = f_clk / (bitrate * 16)
     */
    prescaler = bus_hz / (bitrate * 16UL);
    if (prescaler == 0U) {
        prescaler = 1U;
    }
    if (prescaler > 512U) {
        return -1;
    }

    nbtp = (((2U - 1U) << FDCAN_NBTP_NTSEG2_SHIFT) |
            ((13U - 1U) << FDCAN_NBTP_NTSEG1_SHIFT) |
            ((prescaler - 1U) << FDCAN_NBTP_NBRP_SHIFT) |
            ((1U - 1U) << FDCAN_NBTP_NSJW_SHIFT));

    fdcan_write32(FDCAN_NBTP_OFFSET, nbtp);
    return 0;
}

static int fdcan_hw_init(const struct can_config *cfg)
{
    uint32_t cccr;
    uint32_t test;
    uint32_t txbc;
    uint32_t rxf0c;

    if (cfg == NULL) {
        return -1;
    }

    if (fdcan_enter_init_mode() != 0) {
        return -1;
    }

    cccr = fdcan_read32(FDCAN_CCCR_OFFSET);
    cccr &= ~(FDCAN_CCCR_ASM | FDCAN_CCCR_MON | FDCAN_CCCR_TEST |
              FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE);

    test = fdcan_read32(FDCAN_TEST_OFFSET);
    test &= ~FDCAN_TEST_LBCK;

    switch (cfg->mode) {
        case CAN_MODE_NORMAL:
            break;
        case CAN_MODE_LOOPBACK_INTERNAL:
            cccr |= FDCAN_CCCR_TEST;
            test |= FDCAN_TEST_LBCK;
            break;
        case CAN_MODE_LISTEN_ONLY:
            cccr |= FDCAN_CCCR_MON;
            break;
        case CAN_MODE_LISTEN_LOOPBACK:
            cccr |= (FDCAN_CCCR_MON | FDCAN_CCCR_TEST);
            test |= FDCAN_TEST_LBCK;
            break;
        default:
            return -1;
    }

    if (cfg->fd_enable) {
        cccr |= FDCAN_CCCR_FDOE;
        if (cfg->brs_enable) {
            cccr |= FDCAN_CCCR_BRSE;
        }
    }

    fdcan_write32(FDCAN_TEST_OFFSET, test);
    fdcan_write32(FDCAN_CCCR_OFFSET, cccr);

    if (fdcan_apply_nominal_timing(cfg->nominal_bitrate) != 0) {
        return -1;
    }

    /*
     * Reject no frame by filter; route all unmatched frames to FIFO0.
     * No dedicated SID/XID list configured in this compact sample.
     */
    fdcan_write32(FDCAN_GFC_OFFSET, 0UL);
    fdcan_write32(FDCAN_SIDFC_OFFSET, 0UL);
    fdcan_write32(FDCAN_XIDFC_OFFSET, 0UL);

    /* 8-byte payload elements for both RX FIFO0 and TX queue. */
    fdcan_write32(FDCAN_RXESC_OFFSET, 0UL);
    fdcan_write32(FDCAN_TXESC_OFFSET, 0UL);

    rxf0c = ((FDCAN_RXFIFO0_START_WORD << FDCAN_RXF0C_F0SA_SHIFT) |
             (FDCAN_RXFIFO0_ELEMS << FDCAN_RXF0C_F0S_SHIFT));
    fdcan_write32(FDCAN_RXF0C_OFFSET, rxf0c);

    txbc = ((FDCAN_TXFIFO_START_WORD << FDCAN_TXBC_TBSA_SHIFT) |
            (0U << FDCAN_TXBC_NDTB_SHIFT) |
            (FDCAN_TXFIFO_ELEMS << FDCAN_TXBC_TFQS_SHIFT));
    fdcan_write32(FDCAN_TXBC_OFFSET, txbc);

    fdcan_write32(FDCAN_IE_OFFSET, 0UL);
    fdcan_write32(FDCAN_IR_OFFSET, FDCAN_IR_ALL);

    if (fdcan_leave_init_mode() != 0) {
        return -1;
    }

    return 0;
}

static drv_status_t stm32u5_fdcan_probe(uint32_t label)
{
    g_can_driver.label = label;

    if (merlin_platform_driver_register(&g_can_driver, label) != STATUS_OK) {
        return DRV_ERROR_NOTREGISTERED;
    }

    return DRV_STATUS_OK;
}

static drv_status_t stm32u5_fdcan_init(uint32_t label, const struct can_config *cfg)
{
    (void)label;

    if (cfg == NULL) {
        return DRV_ERROR_INVPARAM;
    }

    if (g_can_driver.devh == 0 || g_can_driver.devinfo == NULL) {
        return DRV_ERROR_NOTREGISTERED;
    }

    if (g_can_ctx.initialized) {
        return DRV_ERROR_INVSTATE;
    }

    if (merlin_platform_driver_map(&g_can_driver) != STATUS_OK) {
        return DRV_ERROR_MAPFAILED;
    }
    g_can_ctx.mapped = true;

    if (merlin_platform_driver_configure_gpio(&g_can_driver) != STATUS_OK) {
        return DRV_ERROR_CONFIGURATION;
    }

    g_can_ctx.msg_ram_base = (size_t)g_can_driver.devinfo->baseaddr + STM32U5_FDCAN_MSGRAM_OFFSET;
    g_can_ctx.cfg = *cfg;

    if (fdcan_hw_init(cfg) != 0) {
        return DRV_ERROR_CONFIGURATION;
    }

    g_can_ctx.initialized = true;
    return DRV_STATUS_OK;
}

static drv_status_t stm32u5_fdcan_send(uint32_t label, const struct can_frame *frame)
{
    uint32_t txfqs;
    uint32_t tx_index;
    uint32_t t0 = 0UL;
    uint32_t t1 = 0UL;
    size_t elem_addr;
    uint8_t payload_len;
    uint8_t dlc;

    (void)label;

    if (!g_can_ctx.initialized || frame == NULL) {
        return DRV_ERROR_INVSTATE;
    }

    if (frame->format == CAN_FRAME_FD && !g_can_ctx.cfg.fd_enable) {
        return DRV_ERROR_UNSUPPORTED_CFG;
    }

    if (frame->id_type == CAN_ID_STANDARD) {
        if (frame->id > 0x7FFUL) {
            return DRV_ERROR_INVPARAM;
        }
        t0 = (frame->id << 18U);
    } else {
        if (frame->id > 0x1FFFFFFFUL) {
            return DRV_ERROR_INVPARAM;
        }
        t0 = (frame->id & 0x1FFFFFFFUL) | FDCAN_FRAME_XTD;
    }

    if (frame->rtr) {
        t0 |= FDCAN_FRAME_RTR;
    }

    txfqs = fdcan_read32(FDCAN_TXFQS_OFFSET);
    if ((txfqs & FDCAN_TXFQS_TFQF) != 0UL ||
        (txfqs & FDCAN_TXFQS_TFFL_MASK) == 0UL) {
        return DRV_ERROR_AGAIN;
    }

    tx_index = (txfqs & FDCAN_TXFQS_TFQPI_MASK) >> FDCAN_TXFQS_TFQPI_SHIFT;
    if (tx_index >= FDCAN_TXFIFO_ELEMS) {
        return DRV_ERROR_BUS;
    }

    payload_len = frame->dlc;
    if (frame->format == CAN_FRAME_CLASSIC) {
        if (payload_len > 8U) {
            payload_len = 8U;
        }
    }

    dlc = can_len_to_dlc(payload_len);
    t1 |= ((uint32_t)dlc << FDCAN_FRAME_DLC_SHIFT);
    if (frame->format == CAN_FRAME_FD) {
        t1 |= FDCAN_FRAME_FDF;
        if (frame->brs && g_can_ctx.cfg.brs_enable) {
            t1 |= FDCAN_FRAME_BRS;
        }
    }

    elem_addr = fdcan_mram_addr(FDCAN_TXFIFO_START_WORD + (tx_index * FDCAN_TXFIFO_ELEM_WORDS));

    merlin_iowrite32(elem_addr + 0U, t0);
    merlin_iowrite32(elem_addr + 4U, t1);

    merlin_iowrite32(elem_addr + 8U,
                     ((uint32_t)frame->data[0]) |
                     ((uint32_t)frame->data[1] << 8U) |
                     ((uint32_t)frame->data[2] << 16U) |
                     ((uint32_t)frame->data[3] << 24U));
    merlin_iowrite32(elem_addr + 12U,
                     ((uint32_t)frame->data[4]) |
                     ((uint32_t)frame->data[5] << 8U) |
                     ((uint32_t)frame->data[6] << 16U) |
                     ((uint32_t)frame->data[7] << 24U));

    fdcan_write32(FDCAN_TXBAR_OFFSET, (1UL << tx_index));

    if (merlin_iopoll32_until_set(fdcan_reg(FDCAN_TXBTO_OFFSET),
                                  (1UL << tx_index),
                                  FDCAN_POLL_RETRIES) != STATUS_OK) {
        return DRV_ERROR_TIMEOUT;
    }

    return DRV_STATUS_OK;
}

static drv_status_t stm32u5_fdcan_recv(uint32_t label, struct can_frame *frame)
{
    uint32_t rxf0s;
    uint32_t fifo_level;
    uint32_t get_index;
    size_t elem_addr;
    uint32_t r0;
    uint32_t r1;
    uint8_t dlc;
    uint8_t payload_len;
    uint32_t data0;
    uint32_t data1;

    (void)label;

    if (!g_can_ctx.initialized || frame == NULL) {
        return DRV_ERROR_INVSTATE;
    }

    rxf0s = fdcan_read32(FDCAN_RXF0S_OFFSET);
    fifo_level = (rxf0s & FDCAN_RXF0S_F0FL_MASK);
    if (fifo_level == 0UL) {
        return DRV_ERROR_AGAIN;
    }

    get_index = (rxf0s & FDCAN_RXF0S_F0GI_MASK) >> FDCAN_RXF0S_F0GI_SHIFT;
    if (get_index >= FDCAN_RXFIFO0_ELEMS) {
        return DRV_ERROR_BUS;
    }

    elem_addr = fdcan_mram_addr(FDCAN_RXFIFO0_START_WORD + (get_index * FDCAN_RXFIFO0_ELEM_WORDS));

    r0 = merlin_ioread32(elem_addr + 0U);
    r1 = merlin_ioread32(elem_addr + 4U);
    data0 = merlin_ioread32(elem_addr + 8U);
    data1 = merlin_ioread32(elem_addr + 12U);

    if ((r0 & FDCAN_FRAME_XTD) != 0UL) {
        frame->id_type = CAN_ID_EXTENDED;
        frame->id = (r0 & 0x1FFFFFFFUL);
    } else {
        frame->id_type = CAN_ID_STANDARD;
        frame->id = ((r0 >> 18U) & 0x7FFUL);
    }

    frame->rtr = ((r0 & FDCAN_FRAME_RTR) != 0UL);
    frame->format = ((r1 & FDCAN_FRAME_FDF) != 0UL) ? CAN_FRAME_FD : CAN_FRAME_CLASSIC;
    frame->brs = ((r1 & FDCAN_FRAME_BRS) != 0UL);

    dlc = (uint8_t)((r1 >> FDCAN_FRAME_DLC_SHIFT) & 0xFU);
    payload_len = g_can_dlc_to_len[dlc];
    if (frame->format == CAN_FRAME_CLASSIC && payload_len > 8U) {
        payload_len = 8U;
    }
    if (payload_len > 8U) {
        payload_len = 8U;
    }
    frame->dlc = payload_len;

    frame->data[0] = (uint8_t)(data0 & 0xFFU);
    frame->data[1] = (uint8_t)((data0 >> 8U) & 0xFFU);
    frame->data[2] = (uint8_t)((data0 >> 16U) & 0xFFU);
    frame->data[3] = (uint8_t)((data0 >> 24U) & 0xFFU);
    frame->data[4] = (uint8_t)(data1 & 0xFFU);
    frame->data[5] = (uint8_t)((data1 >> 8U) & 0xFFU);
    frame->data[6] = (uint8_t)((data1 >> 16U) & 0xFFU);
    frame->data[7] = (uint8_t)((data1 >> 24U) & 0xFFU);

    fdcan_write32(FDCAN_RXF0A_OFFSET, get_index);

    return DRV_STATUS_OK;
}

static drv_status_t stm32u5_fdcan_release(uint32_t label)
{
    (void)label;

    if (!g_can_ctx.mapped) {
        return DRV_ERROR_INVSTATE;
    }

    if (fdcan_enter_init_mode() != 0) {
        return DRV_ERROR_CONFIGURATION;
    }

    fdcan_write32(FDCAN_IE_OFFSET, 0UL);
    fdcan_write32(FDCAN_IR_OFFSET, FDCAN_IR_ALL);

    if (merlin_platform_driver_unmap(&g_can_driver) != STATUS_OK) {
        return DRV_ERROR_MAPFAILED;
    }

    g_can_ctx.initialized = false;
    g_can_ctx.mapped = false;

    return DRV_STATUS_OK;
}

drv_status_t can_probe(uint32_t label) __attribute__((alias("stm32u5_fdcan_probe")));
drv_status_t can_init(uint32_t label, const struct can_config *cfg) __attribute__((alias("stm32u5_fdcan_init")));
drv_status_t can_send(uint32_t label, const struct can_frame *frame) __attribute__((alias("stm32u5_fdcan_send")));
drv_status_t can_recv(uint32_t label, struct can_frame *frame) __attribute__((alias("stm32u5_fdcan_recv")));
drv_status_t can_release(uint32_t label) __attribute__((alias("stm32u5_fdcan_release")));
