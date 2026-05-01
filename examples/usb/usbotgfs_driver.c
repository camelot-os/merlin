// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 H2Lab Development Team

/*
 * Minimal STM32 OTG FS (DWC2-like) device-mode driver example for Merlin.
 *
 * This implementation intentionally focuses on a small, practical subset:
 * - probe/map/gpio setup through Merlin platform APIs
 * - core reset and device-mode bring-up
 * - EP0 baseline setup + generic IN/OUT endpoint configuration hooks
 * - polled FIFO read/write helpers used by IRQ-driven completion
 */

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include <merlin/io.h>
#include <merlin/helpers.h>

#include "usbotgfs_driver.h"

#define USBOTGFS_NUM_EPS 8U
#define USBOTGFS_DIR_IN_IDX 0U
#define USBOTGFS_DIR_OUT_IDX 1U

#define USBOTGFS_TIMEOUT_POLLS 100000U

/* Global register space */
#define USB_GAHBCFG_OFFSET     0x008U
#define USB_GUSBCFG_OFFSET     0x00CU
#define USB_GRSTCTL_OFFSET     0x010U
#define USB_GINTSTS_OFFSET     0x014U
#define USB_GINTMSK_OFFSET     0x018U
#define USB_GRXSTSP_OFFSET     0x020U
#define USB_GRXFSIZ_OFFSET     0x024U
#define USB_GNPTXFSIZ_OFFSET   0x028U
#define USB_GCCFG_OFFSET       0x038U
#define USB_DIEPTXF_OFFSET(n)  (0x104U + (((n) - 1U) * 4U))

/* Device register space */
#define USB_DCFG_OFFSET        0x800U
#define USB_DCTL_OFFSET        0x804U
#define USB_DSTS_OFFSET        0x808U
#define USB_DIEPMSK_OFFSET     0x810U
#define USB_DOEPMSK_OFFSET     0x814U
#define USB_DAINT_OFFSET       0x818U
#define USB_DAINTMSK_OFFSET    0x81CU
#define USB_DVBUSDIS_OFFSET    0x828U
#define USB_DIEPEMPMSK_OFFSET  0x834U

/* IN endpoint registers */
#define USB_DIEP_BASE          0x900U
#define USB_DIEP_STRIDE        0x020U
#define USB_DIEPCTL_OFFSET     0x000U
#define USB_DIEPINT_OFFSET     0x008U
#define USB_DIEPTSIZ_OFFSET    0x010U
#define USB_DTXFSTS_OFFSET     0x018U

/* OUT endpoint registers */
#define USB_DOEP_BASE          0xB00U
#define USB_DOEP_STRIDE        0x020U
#define USB_DOEPCTL_OFFSET     0x000U
#define USB_DOEPINT_OFFSET     0x008U
#define USB_DOEPTSIZ_OFFSET    0x010U

/* FIFO base (Rx FIFO = FIFO0) */
#define USB_FIFO_BASE_OFFSET   0x1000U
#define USB_FIFO_STRIDE        0x1000U

/* GAHBCFG */
#define USB_GAHBCFG_GINT       (1UL << 0)

/* GUSBCFG */
#define USB_GUSBCFG_PHYSEL     (1UL << 6)
#define USB_GUSBCFG_TRDT_SHIFT 10U
#define USB_GUSBCFG_TRDT_MASK  (0xFUL << USB_GUSBCFG_TRDT_SHIFT)
#define USB_GUSBCFG_FDMOD      (1UL << 30)

/* GRSTCTL */
#define USB_GRSTCTL_CSRST      (1UL << 0)
#define USB_GRSTCTL_RXFFLSH    (1UL << 4)
#define USB_GRSTCTL_TXFFLSH    (1UL << 5)
#define USB_GRSTCTL_TXFNUM_SHIFT 6U
#define USB_GRSTCTL_TXFNUM_ALL 0x10UL
#define USB_GRSTCTL_AHBIDL     (1UL << 31)

/* GINTSTS/GINTMSK */
#define USB_GINTSTS_RXFLVL     (1UL << 4)
#define USB_GINTSTS_ESUSP      (1UL << 10)
#define USB_GINTSTS_USBSUSP    (1UL << 11)
#define USB_GINTSTS_USBRST     (1UL << 12)
#define USB_GINTSTS_ENUMDNE    (1UL << 13)
#define USB_GINTSTS_IEPINT     (1UL << 18)
#define USB_GINTSTS_OEPINT     (1UL << 19)
#define USB_GINTSTS_WKUINT     (1UL << 31)

/* GRXSTSP parsing */
#define USB_GRXSTSP_EPNUM_MASK      0x0FUL
#define USB_GRXSTSP_BCNT_SHIFT      4U
#define USB_GRXSTSP_BCNT_MASK       (0x7FFUL << USB_GRXSTSP_BCNT_SHIFT)
#define USB_GRXSTSP_PKTSTS_SHIFT    17U
#define USB_GRXSTSP_PKTSTS_MASK     (0x0FUL << USB_GRXSTSP_PKTSTS_SHIFT)

#define USB_PKTSTS_OUT_RX           0x2U
#define USB_PKTSTS_OUT_DONE         0x3U
#define USB_PKTSTS_SETUP_DONE       0x4U
#define USB_PKTSTS_SETUP_RX         0x6U

/* GCCFG */
#define USB_GCCFG_PWRDWN       (1UL << 16)
#define USB_GCCFG_VBUSBSEN     (1UL << 19)

/* DCFG */
#define USB_DCFG_DSPD_MASK     0x3UL
#define USB_DCFG_DSPD_FS_48MHZ 0x3UL
#define USB_DCFG_DAD_SHIFT     4U
#define USB_DCFG_DAD_MASK      (0x7FUL << USB_DCFG_DAD_SHIFT)

/* DCTL */
#define USB_DCTL_SDIS          (1UL << 1)
#define USB_DCTL_SGINAK        (1UL << 7)
#define USB_DCTL_CGINAK        (1UL << 8)
#define USB_DCTL_SGONAK        (1UL << 9)
#define USB_DCTL_CGONAK        (1UL << 10)

/* DSTS */
#define USB_DSTS_ENUMSPD_SHIFT 1U
#define USB_DSTS_ENUMSPD_MASK  (0x3UL << USB_DSTS_ENUMSPD_SHIFT)
#define USB_DSTS_ENUMSPD_HS    0x0UL
#define USB_DSTS_ENUMSPD_FS1   0x1UL
#define USB_DSTS_ENUMSPD_FS2   0x3UL

/* DIEP/DOEP common CTL bits */
#define USB_DXEPCTL_MPSIZ_MASK     0x7FFUL
#define USB_DXEPCTL_USBAEP         (1UL << 15)
#define USB_DXEPCTL_EPTYP_SHIFT    18U
#define USB_DXEPCTL_EPTYP_MASK     (0x3UL << USB_DXEPCTL_EPTYP_SHIFT)
#define USB_DXEPCTL_STALL          (1UL << 21)
#define USB_DXEPCTL_TXFNUM_SHIFT   22U
#define USB_DXEPCTL_TXFNUM_MASK    (0xFUL << USB_DXEPCTL_TXFNUM_SHIFT)
#define USB_DXEPCTL_CNAK           (1UL << 26)
#define USB_DXEPCTL_SNAK           (1UL << 27)
#define USB_DXEPCTL_SD0PID_SEVNFRM (1UL << 28)
#define USB_DXEPCTL_SD1PID_SODDFRM (1UL << 29)
#define USB_DXEPCTL_EPDIS          (1UL << 30)
#define USB_DXEPCTL_EPENA          (1UL << 31)

/* DIEPTSIZ/DOEPTSIZ */
#define USB_DXEPTSIZ_XFRSIZ_MASK   0x7FFFFUL
#define USB_DXEPTSIZ_PKTCNT_SHIFT  19U
#define USB_DXEPTSIZ_PKTCNT_MASK   (0x3FFUL << USB_DXEPTSIZ_PKTCNT_SHIFT)
#define USB_DOEPTSIZ0_STUPCNT_MASK (0x3UL << 29)
#define USB_DOEPTSIZ0_STUPCNT_3    (0x3UL << 29)

/* DIEPINT/DOEPINT */
#define USB_DIEPINT_XFRC           (1UL << 0)
#define USB_DOEPINT_XFRC           (1UL << 0)
#define USB_DOEPINT_STUP           (1UL << 3)

/* FIFO sizing (FS core depth is 320 words on STM32 OTG FS) */
#define USB_FIFO_TOTAL_WORDS       320U
#define USB_RX_FIFO_WORDS          128U
#define USB_NP_TX_FIFO_WORDS       64U

struct usbotgfs_rx_ctx {
    uint8_t *dst;
    uint32_t expected;
    uint32_t received;
};

static int usbotgfs_isr(void *self, uint32_t IRQn);

static struct platform_device_driver g_usbotgfs_driver = {
    .devh = 0,
    .label = 0,
    .devinfo = NULL,
    .name = "stm32 usbotgfs minimal driver",
    .compatible = "st,stm32-otgfs",
    .driver_fops = NULL,
    .platform_fops = {
        .isr = usbotgfs_isr,
    },
    .type = DEVICE_TYPE_USB,
};

static bool g_initialized;
static bool g_mapped;
static bool g_global_stall;

static usbotgfs_port_speed_t g_port_speed = USBOTG_FS_PORT_SPEED_UNKNOWN;

static usbotgfs_ep_state_t g_ep_state[USBOTGFS_NUM_EPS][2];
static usbotgfs_epx_mpsize_t g_ep_mpsize[USBOTGFS_NUM_EPS][2];
static usbotgfs_ep_type_t g_ep_type[USBOTGFS_NUM_EPS][2];
static usbotgfs_ioep_handler_t g_ep_handler[USBOTGFS_NUM_EPS][2];
static uint32_t g_ep_last_tx_size[USBOTGFS_NUM_EPS];
static struct usbotgfs_rx_ctx g_ep_rx[USBOTGFS_NUM_EPS];

static inline size_t usbotgfs_reg(size_t offset)
{
    return ((size_t)g_usbotgfs_driver.devinfo->baseaddr + offset);
}

static inline uint32_t usbotgfs_read32(size_t offset)
{
    return merlin_ioread32(usbotgfs_reg(offset));
}

static inline void usbotgfs_write32(size_t offset, uint32_t value)
{
    merlin_iowrite32(usbotgfs_reg(offset), value);
}

static inline size_t usbotgfs_diep_offset(uint8_t ep, size_t reg)
{
    return USB_DIEP_BASE + ((size_t)ep * USB_DIEP_STRIDE) + reg;
}

static inline size_t usbotgfs_doep_offset(uint8_t ep, size_t reg)
{
    return USB_DOEP_BASE + ((size_t)ep * USB_DOEP_STRIDE) + reg;
}

static inline size_t usbotgfs_fifo_offset(uint8_t fifo_idx)
{
    return USB_FIFO_BASE_OFFSET + ((size_t)fifo_idx * USB_FIFO_STRIDE);
}

static inline bool usbotgfs_is_ready(void)
{
    return (g_usbotgfs_driver.devh != 0) && (g_usbotgfs_driver.devinfo != NULL);
}

static int usbotgfs_dir_to_index(usbotgfs_ep_dir_t dir)
{
    switch (dir) {
        case USBOTG_FS_EP_DIR_IN:
            return USBOTGFS_DIR_IN_IDX;
        case USBOTG_FS_EP_DIR_OUT:
            return USBOTGFS_DIR_OUT_IDX;
        default:
            return -1;
    }
}

static uint32_t usbotgfs_ep0_mps_to_field(uint16_t mps)
{
    switch (mps) {
        case 64U:
            return USBOTG_FS_EP0_MPSIZE_64BYTES;
        case 32U:
            return USBOTG_FS_EP0_MPSIZE_32BYTES;
        case 16U:
            return USBOTG_FS_EP0_MPSIZE_16BYTES;
        case 8U:
            return USBOTG_FS_EP0_MPSIZE_8BYTES;
        default:
            return USBOTG_FS_EP0_MPSIZE_64BYTES;
    }
}

static uint16_t usbotgfs_ep_get_mps(uint8_t ep, usbotgfs_ep_dir_t dir)
{
    const int idx = usbotgfs_dir_to_index(dir);

    if (idx < 0 || ep >= USBOTGFS_NUM_EPS) {
        return 64U;
    }
    if (g_ep_mpsize[ep][idx] == 0U) {
        return 64U;
    }
    return (uint16_t)g_ep_mpsize[ep][idx];
}

static void usbotgfs_notify(uint8_t ep, usbotgfs_ep_dir_t dir, uint32_t size)
{
    const int idx = usbotgfs_dir_to_index(dir);
    usbotgfs_ioep_handler_t handler;

    if (idx < 0 || ep >= USBOTGFS_NUM_EPS) {
        return;
    }

    handler = g_ep_handler[ep][idx];
    if (handler != NULL) {
        (void)handler(g_usbotgfs_driver.devh, (size_t)size, ep);
    }
}

static int usbotgfs_wait_grstctl_clear(uint32_t mask)
{
    for (uint32_t i = 0U; i < USBOTGFS_TIMEOUT_POLLS; i++) {
        if ((usbotgfs_read32(USB_GRSTCTL_OFFSET) & mask) == 0UL) {
            return 0;
        }
    }
    return -1;
}

static int usbotgfs_wait_ahb_idle(void)
{
    for (uint32_t i = 0U; i < USBOTGFS_TIMEOUT_POLLS; i++) {
        if ((usbotgfs_read32(USB_GRSTCTL_OFFSET) & USB_GRSTCTL_AHBIDL) != 0UL) {
            return 0;
        }
    }
    return -1;
}

static int usbotgfs_core_reset(void)
{
    uint32_t rst;

    if (usbotgfs_wait_ahb_idle() != 0) {
        return -1;
    }

    rst = usbotgfs_read32(USB_GRSTCTL_OFFSET);
    rst |= USB_GRSTCTL_CSRST;
    usbotgfs_write32(USB_GRSTCTL_OFFSET, rst);

    if (usbotgfs_wait_grstctl_clear(USB_GRSTCTL_CSRST) != 0) {
        return -1;
    }

    return usbotgfs_wait_ahb_idle();
}

static int usbotgfs_flush_rx_fifo(void)
{
    uint32_t rst = usbotgfs_read32(USB_GRSTCTL_OFFSET);

    rst |= USB_GRSTCTL_RXFFLSH;
    usbotgfs_write32(USB_GRSTCTL_OFFSET, rst);

    return usbotgfs_wait_grstctl_clear(USB_GRSTCTL_RXFFLSH);
}

static int usbotgfs_flush_tx_fifo(uint32_t fifo_num)
{
    uint32_t rst = usbotgfs_read32(USB_GRSTCTL_OFFSET);

    rst &= ~((uint32_t)0x1FUL << USB_GRSTCTL_TXFNUM_SHIFT);
    rst |= (fifo_num << USB_GRSTCTL_TXFNUM_SHIFT);
    rst |= USB_GRSTCTL_TXFFLSH;
    usbotgfs_write32(USB_GRSTCTL_OFFSET, rst);

    return usbotgfs_wait_grstctl_clear(USB_GRSTCTL_TXFFLSH);
}

static void usbotgfs_update_port_speed(void)
{
    const uint32_t enumspd = (usbotgfs_read32(USB_DSTS_OFFSET) & USB_DSTS_ENUMSPD_MASK) >> USB_DSTS_ENUMSPD_SHIFT;

    switch (enumspd) {
        case USB_DSTS_ENUMSPD_HS:
            g_port_speed = USBOTG_FS_PORT_SPEED_HIGH;
            break;
        case USB_DSTS_ENUMSPD_FS1:
        case USB_DSTS_ENUMSPD_FS2:
            g_port_speed = USBOTG_FS_PORT_SPEED_FULL;
            break;
        default:
            g_port_speed = USBOTG_FS_PORT_SPEED_UNKNOWN;
            break;
    }
}

static void usbotgfs_reset_runtime_state(void)
{
    for (size_t ep = 0U; ep < USBOTGFS_NUM_EPS; ep++) {
        g_ep_last_tx_size[ep] = 0U;
        g_ep_rx[ep].dst = NULL;
        g_ep_rx[ep].expected = 0U;
        g_ep_rx[ep].received = 0U;
        for (size_t dir = 0U; dir < 2U; dir++) {
            g_ep_state[ep][dir] = USBOTG_FS_EP_STATE_IDLE;
            if (g_ep_mpsize[ep][dir] == 0U) {
                g_ep_mpsize[ep][dir] = USBOTG_FS_EPx_MPSIZE_64BYTES;
            }
            g_ep_type[ep][dir] = USBOTG_FS_EP_TYPE_CONTROL;
        }
    }
}

static void usbotgfs_arm_ep0_setup_reception(void)
{
    uint32_t doepctl0;

    usbotgfs_write32(usbotgfs_doep_offset(0U, USB_DOEPTSIZ_OFFSET),
                     USB_DOEPTSIZ0_STUPCNT_3 |
                     (1UL << USB_DXEPTSIZ_PKTCNT_SHIFT) |
                     24UL);

    doepctl0 = usbotgfs_read32(usbotgfs_doep_offset(0U, USB_DOEPCTL_OFFSET));
    doepctl0 |= USB_DXEPCTL_USBAEP | USB_DXEPCTL_CNAK | USB_DXEPCTL_EPENA;
    doepctl0 &= ~USB_DXEPCTL_EPDIS;
    usbotgfs_write32(usbotgfs_doep_offset(0U, USB_DOEPCTL_OFFSET), doepctl0);
}

static int usbotgfs_configure_fifos(void)
{
    static const uint16_t tx_fifo_words[USBOTGFS_NUM_EPS - 1U] = { 32U, 32U, 32U, 8U, 8U, 8U, 8U };
    uint32_t tx_start = USB_RX_FIFO_WORDS + USB_NP_TX_FIFO_WORDS;

    usbotgfs_write32(USB_GRXFSIZ_OFFSET, USB_RX_FIFO_WORDS);
    usbotgfs_write32(USB_GNPTXFSIZ_OFFSET, ((uint32_t)USB_NP_TX_FIFO_WORDS << 16) | USB_RX_FIFO_WORDS);

    for (uint8_t ep = 1U; ep < USBOTGFS_NUM_EPS; ep++) {
        const uint32_t depth = tx_fifo_words[ep - 1U];

        usbotgfs_write32(USB_DIEPTXF_OFFSET(ep), (depth << 16) | tx_start);
        tx_start += depth;
    }

    if (tx_start > USB_FIFO_TOTAL_WORDS) {
        return -1;
    }

    return 0;
}

static void usbotgfs_configure_device_interrupts(void)
{
    const uint32_t gintmsk = USB_GINTSTS_USBRST |
                             USB_GINTSTS_ENUMDNE |
                             USB_GINTSTS_USBSUSP |
                             USB_GINTSTS_ESUSP |
                             USB_GINTSTS_WKUINT |
                             USB_GINTSTS_RXFLVL |
                             USB_GINTSTS_IEPINT |
                             USB_GINTSTS_OEPINT;

    usbotgfs_write32(USB_DIEPMSK_OFFSET, USB_DIEPINT_XFRC);
    usbotgfs_write32(USB_DOEPMSK_OFFSET, USB_DOEPINT_XFRC | USB_DOEPINT_STUP);
    usbotgfs_write32(USB_DAINTMSK_OFFSET, (1UL << 0) | (1UL << 16));
    usbotgfs_write32(USB_DIEPEMPMSK_OFFSET, 0UL);

    usbotgfs_write32(USB_GINTSTS_OFFSET, 0xFFFFFFFFUL);
    usbotgfs_write32(USB_GINTMSK_OFFSET, gintmsk);
}

static int usbotgfs_core_device_init(void)
{
    uint32_t gahbcfg;
    uint32_t gusbcfg;
    uint32_t gccfg;
    uint32_t dcfg;
    uint32_t dctl;

    if (!usbotgfs_is_ready()) {
        return -1;
    }

    if (usbotgfs_core_reset() != 0) {
        return -1;
    }

    gusbcfg = usbotgfs_read32(USB_GUSBCFG_OFFSET);
    gusbcfg |= USB_GUSBCFG_PHYSEL | USB_GUSBCFG_FDMOD;
    gusbcfg &= ~USB_GUSBCFG_TRDT_MASK;
    gusbcfg |= (6UL << USB_GUSBCFG_TRDT_SHIFT);
    usbotgfs_write32(USB_GUSBCFG_OFFSET, gusbcfg);

    gccfg = usbotgfs_read32(USB_GCCFG_OFFSET);
    gccfg |= USB_GCCFG_PWRDWN | USB_GCCFG_VBUSBSEN;
    usbotgfs_write32(USB_GCCFG_OFFSET, gccfg);

    if (usbotgfs_configure_fifos() != 0) {
        return -1;
    }
    if (usbotgfs_flush_rx_fifo() != 0) {
        return -1;
    }
    if (usbotgfs_flush_tx_fifo(USB_GRSTCTL_TXFNUM_ALL) != 0) {
        return -1;
    }

    dcfg = usbotgfs_read32(USB_DCFG_OFFSET);
    dcfg &= ~USB_DCFG_DSPD_MASK;
    dcfg |= USB_DCFG_DSPD_FS_48MHZ;
    dcfg &= ~USB_DCFG_DAD_MASK;
    usbotgfs_write32(USB_DCFG_OFFSET, dcfg);

    usbotgfs_write32(USB_DVBUSDIS_OFFSET, 0UL);

    /* EP0 baseline configuration */
    usbotgfs_write32(usbotgfs_diep_offset(0U, USB_DIEPCTL_OFFSET),
                     USB_DXEPCTL_USBAEP | usbotgfs_ep0_mps_to_field(64U));
    usbotgfs_write32(usbotgfs_doep_offset(0U, USB_DOEPCTL_OFFSET),
                     USB_DXEPCTL_USBAEP | usbotgfs_ep0_mps_to_field(64U));

    usbotgfs_arm_ep0_setup_reception();
    usbotgfs_configure_device_interrupts();

    gahbcfg = usbotgfs_read32(USB_GAHBCFG_OFFSET);
    gahbcfg |= USB_GAHBCFG_GINT;
    usbotgfs_write32(USB_GAHBCFG_OFFSET, gahbcfg);

    dctl = usbotgfs_read32(USB_DCTL_OFFSET);
    dctl &= ~USB_DCTL_SDIS;
    usbotgfs_write32(USB_DCTL_OFFSET, dctl);

    usbotgfs_update_port_speed();

    return 0;
}

static void usbotgfs_fifo_write(uint8_t fifo_idx, const uint8_t *src, uint32_t size)
{
    const size_t fifo_addr = usbotgfs_reg(usbotgfs_fifo_offset(fifo_idx));
    const uint32_t words = (size + 3U) / 4U;

    for (uint32_t i = 0U; i < words; i++) {
        uint32_t word = 0UL;

        for (uint32_t b = 0U; b < 4U; b++) {
            const uint32_t idx = (i * 4U) + b;
            if (idx < size) {
                word |= ((uint32_t)src[idx] << (8U * b));
            }
        }
        merlin_iowrite32(fifo_addr, word);
    }
}

static void usbotgfs_fifo_read_to_ep_buffer(uint8_t ep, uint32_t size)
{
    const size_t fifo_addr = usbotgfs_reg(usbotgfs_fifo_offset(0U));
    const uint32_t words = (size + 3U) / 4U;
    uint32_t remaining = size;

    for (uint32_t i = 0U; i < words; i++) {
        const uint32_t word = merlin_ioread32(fifo_addr);

        for (uint32_t b = 0U; b < 4U; b++) {
            uint8_t v;

            if (remaining == 0U) {
                break;
            }
            v = (uint8_t)((word >> (8U * b)) & 0xFFU);
            if (ep < USBOTGFS_NUM_EPS &&
                g_ep_rx[ep].dst != NULL &&
                g_ep_rx[ep].received < g_ep_rx[ep].expected) {
                g_ep_rx[ep].dst[g_ep_rx[ep].received++] = v;
            }
            remaining--;
        }
    }
}

static void usbotgfs_handle_rxflvl(void)
{
    while ((usbotgfs_read32(USB_GINTSTS_OFFSET) & USB_GINTSTS_RXFLVL) != 0UL) {
        const uint32_t rxstsp = usbotgfs_read32(USB_GRXSTSP_OFFSET);
        const uint8_t ep = (uint8_t)(rxstsp & USB_GRXSTSP_EPNUM_MASK);
        const uint32_t bcnt = (rxstsp & USB_GRXSTSP_BCNT_MASK) >> USB_GRXSTSP_BCNT_SHIFT;
        const uint32_t pktsts = (rxstsp & USB_GRXSTSP_PKTSTS_MASK) >> USB_GRXSTSP_PKTSTS_SHIFT;

        if (ep >= USBOTGFS_NUM_EPS) {
            continue;
        }

        switch (pktsts) {
            case USB_PKTSTS_SETUP_RX:
                g_ep_state[ep][USBOTGFS_DIR_OUT_IDX] = USBOTG_FS_EP_STATE_SETUP_WIP;
                if (bcnt != 0U) {
                    usbotgfs_fifo_read_to_ep_buffer(ep, bcnt);
                }
                break;
            case USB_PKTSTS_OUT_RX:
                g_ep_state[ep][USBOTGFS_DIR_OUT_IDX] = USBOTG_FS_EP_STATE_DATA_OUT_WIP;
                if (bcnt != 0U) {
                    usbotgfs_fifo_read_to_ep_buffer(ep, bcnt);
                }
                break;
            case USB_PKTSTS_SETUP_DONE:
                g_ep_state[ep][USBOTGFS_DIR_OUT_IDX] = USBOTG_FS_EP_STATE_SETUP;
                break;
            case USB_PKTSTS_OUT_DONE:
                g_ep_state[ep][USBOTGFS_DIR_OUT_IDX] = USBOTG_FS_EP_STATE_DATA_OUT;
                break;
            default:
                break;
        }
    }
}

static void usbotgfs_handle_iepint(void)
{
    const uint32_t daint = usbotgfs_read32(USB_DAINT_OFFSET) & usbotgfs_read32(USB_DAINTMSK_OFFSET);
    const uint32_t in_pending = daint & 0xFFFFUL;

    for (uint8_t ep = 0U; ep < USBOTGFS_NUM_EPS; ep++) {
        const uint32_t ep_mask = (1UL << ep);

        if ((in_pending & ep_mask) == 0UL) {
            continue;
        }

        const size_t diepint_off = usbotgfs_diep_offset(ep, USB_DIEPINT_OFFSET);
        const uint32_t diepint = usbotgfs_read32(diepint_off);

        usbotgfs_write32(diepint_off, diepint);

        if ((diepint & USB_DIEPINT_XFRC) != 0UL) {
            g_ep_state[ep][USBOTGFS_DIR_IN_IDX] = USBOTG_FS_EP_STATE_DATA_IN;
            usbotgfs_notify(ep, USBOTG_FS_EP_DIR_IN, g_ep_last_tx_size[ep]);
            (void)usbctrl_handle_inepevent(g_usbotgfs_driver.label, g_ep_last_tx_size[ep], ep);
        }
    }
}

static void usbotgfs_handle_oepint(void)
{
    const uint32_t daint = usbotgfs_read32(USB_DAINT_OFFSET) & usbotgfs_read32(USB_DAINTMSK_OFFSET);
    const uint32_t out_pending = (daint >> 16U) & 0xFFFFUL;

    for (uint8_t ep = 0U; ep < USBOTGFS_NUM_EPS; ep++) {
        const uint32_t ep_mask = (1UL << ep);

        if ((out_pending & ep_mask) == 0UL) {
            continue;
        }

        const size_t doepint_off = usbotgfs_doep_offset(ep, USB_DOEPINT_OFFSET);
        const uint32_t doepint = usbotgfs_read32(doepint_off);

        usbotgfs_write32(doepint_off, doepint);

        if ((doepint & USB_DOEPINT_STUP) != 0UL) {
            g_ep_state[ep][USBOTGFS_DIR_OUT_IDX] = USBOTG_FS_EP_STATE_SETUP;
            usbotgfs_notify(ep, USBOTG_FS_EP_DIR_OUT, g_ep_rx[ep].received);
            (void)usbctrl_handle_outepevent(g_usbotgfs_driver.label, g_ep_rx[ep].received, ep);
            g_ep_rx[ep].received = 0U;
            if (ep == 0U) {
                usbotgfs_arm_ep0_setup_reception();
            }
        }

        if ((doepint & USB_DOEPINT_XFRC) != 0UL) {
            g_ep_state[ep][USBOTGFS_DIR_OUT_IDX] = USBOTG_FS_EP_STATE_DATA_OUT;
            usbotgfs_notify(ep, USBOTG_FS_EP_DIR_OUT, g_ep_rx[ep].received);
            (void)usbctrl_handle_outepevent(g_usbotgfs_driver.label, g_ep_rx[ep].received, ep);
            g_ep_rx[ep].received = 0U;
        }
    }
}

static int usbotgfs_isr(void *self, uint32_t IRQn)
{
    uint32_t gintsts;
    uint32_t clear_mask = 0UL;

    (void)self;

    if (!usbotgfs_is_ready()) {
        return -1;
    }

    gintsts = usbotgfs_read32(USB_GINTSTS_OFFSET) & usbotgfs_read32(USB_GINTMSK_OFFSET);

    if ((gintsts & USB_GINTSTS_USBRST) != 0UL) {
        usbotgfs_reset_runtime_state();
        usbotgfs_arm_ep0_setup_reception();
        (void)usbctrl_handle_reset(g_usbotgfs_driver.label);
        clear_mask |= USB_GINTSTS_USBRST;
    }

    if ((gintsts & USB_GINTSTS_ENUMDNE) != 0UL) {
        usbotgfs_update_port_speed();
        clear_mask |= USB_GINTSTS_ENUMDNE;
    }

    if ((gintsts & USB_GINTSTS_RXFLVL) != 0UL) {
        usbotgfs_handle_rxflvl();
    }

    if ((gintsts & USB_GINTSTS_IEPINT) != 0UL) {
        usbotgfs_handle_iepint();
        clear_mask |= USB_GINTSTS_IEPINT;
    }

    if ((gintsts & USB_GINTSTS_OEPINT) != 0UL) {
        usbotgfs_handle_oepint();
        clear_mask |= USB_GINTSTS_OEPINT;
    }

    if ((gintsts & USB_GINTSTS_ESUSP) != 0UL) {
        (void)usbctrl_handle_earlysuspend(g_usbotgfs_driver.label);
        clear_mask |= USB_GINTSTS_ESUSP;
    }

    if ((gintsts & USB_GINTSTS_USBSUSP) != 0UL) {
        (void)usbctrl_handle_usbsuspend(g_usbotgfs_driver.label);
        clear_mask |= USB_GINTSTS_USBSUSP;
    }

    if ((gintsts & USB_GINTSTS_WKUINT) != 0UL) {
        (void)usbctrl_handle_wakeup(g_usbotgfs_driver.label);
        clear_mask |= USB_GINTSTS_WKUINT;
    }

    if (clear_mask != 0UL) {
        usbotgfs_write32(USB_GINTSTS_OFFSET, clear_mask);
    }

    if (merlin_platform_acknowledge_irq(&g_usbotgfs_driver, IRQn) != STATUS_OK) {
        return -1;
    }

    return 0;
}

int usbotgfs_probe(uint32_t label)
{
    if (merlin_platform_driver_register(&g_usbotgfs_driver, label) != STATUS_OK) {
        return -1;
    }
    return 0;
}

int usbotgfs_init(enum usb_otg_mode mode, enum usb_maximum_speed max_speed)
{
    (void)max_speed;

    if (mode != USB_OTG_MODE_DEVICE) {
        return -1;
    }
    if (!usbotgfs_is_ready()) {
        return -1;
    }

    if (!g_mapped) {
        if (merlin_platform_driver_map(&g_usbotgfs_driver) != STATUS_OK) {
            return -1;
        }
        if (merlin_platform_driver_configure_gpio(&g_usbotgfs_driver) != STATUS_OK) {
            return -1;
        }
        g_mapped = true;
    }

    usbotgfs_reset_runtime_state();

    if (usbotgfs_core_device_init() != 0) {
        return -1;
    }

    g_initialized = true;
    return 0;
}

int usbotgfs_send_data(uint8_t *src, uint32_t size, uint8_t ep)
{
    uint32_t pktcnt;
    uint32_t xfrsize;
    uint32_t dieptsiz;
    uint32_t diepctl;
    uint32_t txfsts;
    uint16_t mps;

    if (!g_initialized || src == NULL || ep >= USBOTGFS_NUM_EPS) {
        return -1;
    }

    if (size == 0U) {
        return usbotgfs_send_zlp(ep);
    }

    mps = usbotgfs_ep_get_mps(ep, USBOTG_FS_EP_DIR_IN);
    pktcnt = (size + (uint32_t)mps - 1U) / (uint32_t)mps;
    xfrsize = size & USB_DXEPTSIZ_XFRSIZ_MASK;

    dieptsiz = xfrsize | ((pktcnt << USB_DXEPTSIZ_PKTCNT_SHIFT) & USB_DXEPTSIZ_PKTCNT_MASK);
    usbotgfs_write32(usbotgfs_diep_offset(ep, USB_DIEPTSIZ_OFFSET), dieptsiz);

    txfsts = usbotgfs_read32(usbotgfs_diep_offset(ep, USB_DTXFSTS_OFFSET)) & 0xFFFFUL;
    if (txfsts < ((size + 3U) / 4U)) {
        return -1;
    }

    usbotgfs_fifo_write(ep, src, size);

    diepctl = usbotgfs_read32(usbotgfs_diep_offset(ep, USB_DIEPCTL_OFFSET));
    diepctl |= USB_DXEPCTL_CNAK | USB_DXEPCTL_EPENA;
    diepctl &= ~USB_DXEPCTL_EPDIS;
    usbotgfs_write32(usbotgfs_diep_offset(ep, USB_DIEPCTL_OFFSET), diepctl);

    g_ep_last_tx_size[ep] = size;
    g_ep_state[ep][USBOTGFS_DIR_IN_IDX] = USBOTG_FS_EP_STATE_DATA_IN_WIP;

    return 0;
}

int usbotgfs_set_recv_fifo(uint8_t *dst, uint32_t size, uint8_t ep)
{
    uint32_t pktcnt;
    uint32_t doeptsiz;
    uint32_t doepctl;
    uint16_t mps;

    if (!g_initialized || ep >= USBOTGFS_NUM_EPS) {
        return -1;
    }

    if ((dst == NULL && size != 0U) || (size > USB_DXEPTSIZ_XFRSIZ_MASK)) {
        return -1;
    }

    g_ep_rx[ep].dst = dst;
    g_ep_rx[ep].expected = size;
    g_ep_rx[ep].received = 0U;

    mps = usbotgfs_ep_get_mps(ep, USBOTG_FS_EP_DIR_OUT);
    pktcnt = (size == 0U) ? 1U : ((size + (uint32_t)mps - 1U) / (uint32_t)mps);
    doeptsiz = (size & USB_DXEPTSIZ_XFRSIZ_MASK) |
               ((pktcnt << USB_DXEPTSIZ_PKTCNT_SHIFT) & USB_DXEPTSIZ_PKTCNT_MASK);

    if (ep == 0U && size == 0U) {
        doeptsiz |= USB_DOEPTSIZ0_STUPCNT_3;
    }

    usbotgfs_write32(usbotgfs_doep_offset(ep, USB_DOEPTSIZ_OFFSET), doeptsiz);

    doepctl = usbotgfs_read32(usbotgfs_doep_offset(ep, USB_DOEPCTL_OFFSET));
    doepctl |= USB_DXEPCTL_CNAK | USB_DXEPCTL_EPENA;
    doepctl &= ~USB_DXEPCTL_EPDIS;
    usbotgfs_write32(usbotgfs_doep_offset(ep, USB_DOEPCTL_OFFSET), doepctl);

    g_ep_state[ep][USBOTGFS_DIR_OUT_IDX] = USBOTG_FS_EP_STATE_DATA_OUT_WIP;

    return 0;
}

int usbotgfs_send_zlp(uint8_t ep)
{
    uint32_t dieptsiz;
    uint32_t diepctl;

    if (!g_initialized || ep >= USBOTGFS_NUM_EPS) {
        return -1;
    }

    dieptsiz = (1UL << USB_DXEPTSIZ_PKTCNT_SHIFT);
    usbotgfs_write32(usbotgfs_diep_offset(ep, USB_DIEPTSIZ_OFFSET), dieptsiz);

    diepctl = usbotgfs_read32(usbotgfs_diep_offset(ep, USB_DIEPCTL_OFFSET));
    diepctl |= USB_DXEPCTL_CNAK | USB_DXEPCTL_EPENA;
    diepctl &= ~USB_DXEPCTL_EPDIS;
    usbotgfs_write32(usbotgfs_diep_offset(ep, USB_DIEPCTL_OFFSET), diepctl);

    g_ep_last_tx_size[ep] = 0U;
    g_ep_state[ep][USBOTGFS_DIR_IN_IDX] = USBOTG_FS_EP_STATE_STATUS;

    return 0;
}

int usbotgfs_global_stall(void)
{
    uint32_t dctl;

    if (!g_initialized) {
        return -1;
    }

    dctl = usbotgfs_read32(USB_DCTL_OFFSET);
    dctl |= USB_DCTL_SGINAK | USB_DCTL_SGONAK;
    usbotgfs_write32(USB_DCTL_OFFSET, dctl);
    g_global_stall = true;

    return 0;
}

int usbotgfs_global_stall_clear(void)
{
    uint32_t dctl;

    if (!g_initialized) {
        return -1;
    }

    dctl = usbotgfs_read32(USB_DCTL_OFFSET);
    dctl |= USB_DCTL_CGINAK | USB_DCTL_CGONAK;
    usbotgfs_write32(USB_DCTL_OFFSET, dctl);
    g_global_stall = false;

    return 0;
}

static int usbotgfs_endpoint_modify_stall(uint8_t ep, usbotgfs_ep_dir_t dir, bool stall)
{
    uint32_t reg;

    if (!g_initialized || ep >= USBOTGFS_NUM_EPS) {
        return -1;
    }

    if (dir == USBOTG_FS_EP_DIR_IN || dir == USBOTG_FS_EP_DIR_BOTH) {
        reg = usbotgfs_read32(usbotgfs_diep_offset(ep, USB_DIEPCTL_OFFSET));
        if (stall) {
            reg |= USB_DXEPCTL_STALL;
            g_ep_state[ep][USBOTGFS_DIR_IN_IDX] = USBOTG_FS_EP_STATE_STALL;
        } else {
            reg &= ~USB_DXEPCTL_STALL;
            g_ep_state[ep][USBOTGFS_DIR_IN_IDX] = USBOTG_FS_EP_STATE_IDLE;
        }
        usbotgfs_write32(usbotgfs_diep_offset(ep, USB_DIEPCTL_OFFSET), reg);
    }

    if (dir == USBOTG_FS_EP_DIR_OUT || dir == USBOTG_FS_EP_DIR_BOTH) {
        reg = usbotgfs_read32(usbotgfs_doep_offset(ep, USB_DOEPCTL_OFFSET));
        if (stall) {
            reg |= USB_DXEPCTL_STALL;
            g_ep_state[ep][USBOTGFS_DIR_OUT_IDX] = USBOTG_FS_EP_STATE_STALL;
        } else {
            reg &= ~USB_DXEPCTL_STALL;
            g_ep_state[ep][USBOTGFS_DIR_OUT_IDX] = USBOTG_FS_EP_STATE_IDLE;
        }
        usbotgfs_write32(usbotgfs_doep_offset(ep, USB_DOEPCTL_OFFSET), reg);
    }

    return 0;
}

int usbotgfs_endpoint_stall(uint8_t ep, usbotgfs_ep_dir_t dir)
{
    return usbotgfs_endpoint_modify_stall(ep, dir, true);
}

int usbotgfs_endpoint_stall_clear(uint8_t ep, usbotgfs_ep_dir_t dir)
{
    return usbotgfs_endpoint_modify_stall(ep, dir, false);
}

int usbotgfs_endpoint_set_nak(uint8_t ep, usbotgfs_ep_dir_t dir)
{
    uint32_t reg;

    if (!g_initialized || ep >= USBOTGFS_NUM_EPS) {
        return -1;
    }

    if (dir == USBOTG_FS_EP_DIR_IN || dir == USBOTG_FS_EP_DIR_BOTH) {
        reg = usbotgfs_read32(usbotgfs_diep_offset(ep, USB_DIEPCTL_OFFSET));
        reg |= USB_DXEPCTL_SNAK;
        usbotgfs_write32(usbotgfs_diep_offset(ep, USB_DIEPCTL_OFFSET), reg);
    }

    if (dir == USBOTG_FS_EP_DIR_OUT || dir == USBOTG_FS_EP_DIR_BOTH) {
        reg = usbotgfs_read32(usbotgfs_doep_offset(ep, USB_DOEPCTL_OFFSET));
        reg |= USB_DXEPCTL_SNAK;
        usbotgfs_write32(usbotgfs_doep_offset(ep, USB_DOEPCTL_OFFSET), reg);
    }

    return 0;
}

int usbotgfs_endpoint_clear_nak(uint8_t ep, usbotgfs_ep_dir_t dir)
{
    uint32_t reg;

    if (!g_initialized || ep >= USBOTGFS_NUM_EPS) {
        return -1;
    }

    if (dir == USBOTG_FS_EP_DIR_IN || dir == USBOTG_FS_EP_DIR_BOTH) {
        reg = usbotgfs_read32(usbotgfs_diep_offset(ep, USB_DIEPCTL_OFFSET));
        reg |= USB_DXEPCTL_CNAK;
        usbotgfs_write32(usbotgfs_diep_offset(ep, USB_DIEPCTL_OFFSET), reg);
    }

    if (dir == USBOTG_FS_EP_DIR_OUT || dir == USBOTG_FS_EP_DIR_BOTH) {
        reg = usbotgfs_read32(usbotgfs_doep_offset(ep, USB_DOEPCTL_OFFSET));
        reg |= USB_DXEPCTL_CNAK;
        usbotgfs_write32(usbotgfs_doep_offset(ep, USB_DOEPCTL_OFFSET), reg);
    }

    return 0;
}

static int usbotgfs_configure_one_ep(uint8_t ep,
                                     usbotgfs_ep_type_t type,
                                     usbotgfs_ep_dir_t dir,
                                     usbotgfs_epx_mpsize_t mpsize,
                                     usbotgfs_ep_toggle_t dtoggle,
                                     usbotgfs_ioep_handler_t handler)
{
    const bool in_dir = (dir == USBOTG_FS_EP_DIR_IN);
    uint32_t reg;
    size_t ctl_off;
    int idx;

    if (ep >= USBOTGFS_NUM_EPS) {
        return -1;
    }

    idx = usbotgfs_dir_to_index(dir);
    if (idx < 0) {
        return -1;
    }

    ctl_off = in_dir ? usbotgfs_diep_offset(ep, USB_DIEPCTL_OFFSET)
                     : usbotgfs_doep_offset(ep, USB_DOEPCTL_OFFSET);

    reg = usbotgfs_read32(ctl_off);
    reg &= ~(USB_DXEPCTL_MPSIZ_MASK |
             USB_DXEPCTL_EPTYP_MASK |
             USB_DXEPCTL_TXFNUM_MASK |
             USB_DXEPCTL_SD0PID_SEVNFRM |
             USB_DXEPCTL_SD1PID_SODDFRM |
             USB_DXEPCTL_STALL |
             USB_DXEPCTL_EPDIS |
             USB_DXEPCTL_EPENA);

    if (ep == 0U) {
        reg |= usbotgfs_ep0_mps_to_field((uint16_t)mpsize);
    } else {
        reg |= ((uint32_t)mpsize & USB_DXEPCTL_MPSIZ_MASK);
        reg |= (((uint32_t)type << USB_DXEPCTL_EPTYP_SHIFT) & USB_DXEPCTL_EPTYP_MASK);
        if (in_dir) {
            reg |= (((uint32_t)ep << USB_DXEPCTL_TXFNUM_SHIFT) & USB_DXEPCTL_TXFNUM_MASK);
        }
    }

    if (dtoggle == USB_FS_DXEPCTL_SD1PID_SODDFRM) {
        reg |= USB_DXEPCTL_SD1PID_SODDFRM;
    } else {
        reg |= USB_DXEPCTL_SD0PID_SEVNFRM;
    }

    reg |= USB_DXEPCTL_USBAEP;
    usbotgfs_write32(ctl_off, reg);

    g_ep_mpsize[ep][idx] = mpsize;
    g_ep_type[ep][idx] = type;
    g_ep_handler[ep][idx] = handler;
    g_ep_state[ep][idx] = USBOTG_FS_EP_STATE_IDLE;

    return 0;
}

int usbotgfs_configure_endpoint(uint8_t ep,
                                usbotgfs_ep_type_t type,
                                usbotgfs_ep_dir_t dir,
                                usbotgfs_epx_mpsize_t mpsize,
                                usbotgfs_ep_toggle_t dtoggle,
                                usbotgfs_ioep_handler_t handler)
{
    if (!g_initialized) {
        return -1;
    }

    if (dir == USBOTG_FS_EP_DIR_BOTH) {
        if (usbotgfs_configure_one_ep(ep, type, USBOTG_FS_EP_DIR_IN, mpsize, dtoggle, handler) != 0) {
            return -1;
        }
        return usbotgfs_configure_one_ep(ep, type, USBOTG_FS_EP_DIR_OUT, mpsize, dtoggle, handler);
    }

    return usbotgfs_configure_one_ep(ep, type, dir, mpsize, dtoggle, handler);
}

int usbotgfs_deconfigure_endpoint(uint8_t ep)
{
    if (!g_initialized || ep >= USBOTGFS_NUM_EPS) {
        return -1;
    }

    (void)usbotgfs_deactivate_endpoint(ep, USBOTG_FS_EP_DIR_BOTH);
    g_ep_handler[ep][USBOTGFS_DIR_IN_IDX] = NULL;
    g_ep_handler[ep][USBOTGFS_DIR_OUT_IDX] = NULL;
    g_ep_state[ep][USBOTGFS_DIR_IN_IDX] = USBOTG_FS_EP_STATE_IDLE;
    g_ep_state[ep][USBOTGFS_DIR_OUT_IDX] = USBOTG_FS_EP_STATE_IDLE;

    return 0;
}

int usbotgfs_activate_endpoint(uint8_t ep, usbotgfs_ep_dir_t dir)
{
    uint32_t reg;

    if (!g_initialized || ep >= USBOTGFS_NUM_EPS) {
        return -1;
    }

    if (dir == USBOTG_FS_EP_DIR_IN || dir == USBOTG_FS_EP_DIR_BOTH) {
        reg = usbotgfs_read32(usbotgfs_diep_offset(ep, USB_DIEPCTL_OFFSET));
        reg |= USB_DXEPCTL_USBAEP | USB_DXEPCTL_CNAK | USB_DXEPCTL_EPENA;
        reg &= ~USB_DXEPCTL_EPDIS;
        usbotgfs_write32(usbotgfs_diep_offset(ep, USB_DIEPCTL_OFFSET), reg);
    }

    if (dir == USBOTG_FS_EP_DIR_OUT || dir == USBOTG_FS_EP_DIR_BOTH) {
        reg = usbotgfs_read32(usbotgfs_doep_offset(ep, USB_DOEPCTL_OFFSET));
        reg |= USB_DXEPCTL_USBAEP | USB_DXEPCTL_CNAK | USB_DXEPCTL_EPENA;
        reg &= ~USB_DXEPCTL_EPDIS;
        usbotgfs_write32(usbotgfs_doep_offset(ep, USB_DOEPCTL_OFFSET), reg);
    }

    return 0;
}

int usbotgfs_deactivate_endpoint(uint8_t ep, usbotgfs_ep_dir_t dir)
{
    uint32_t reg;

    if (!g_initialized || ep >= USBOTGFS_NUM_EPS) {
        return -1;
    }

    if (dir == USBOTG_FS_EP_DIR_IN || dir == USBOTG_FS_EP_DIR_BOTH) {
        reg = usbotgfs_read32(usbotgfs_diep_offset(ep, USB_DIEPCTL_OFFSET));
        reg |= USB_DXEPCTL_SNAK | USB_DXEPCTL_EPDIS;
        reg &= ~USB_DXEPCTL_EPENA;
        usbotgfs_write32(usbotgfs_diep_offset(ep, USB_DIEPCTL_OFFSET), reg);
        g_ep_state[ep][USBOTGFS_DIR_IN_IDX] = USBOTG_FS_EP_STATE_IDLE;
    }

    if (dir == USBOTG_FS_EP_DIR_OUT || dir == USBOTG_FS_EP_DIR_BOTH) {
        reg = usbotgfs_read32(usbotgfs_doep_offset(ep, USB_DOEPCTL_OFFSET));
        reg |= USB_DXEPCTL_SNAK | USB_DXEPCTL_EPDIS;
        reg &= ~USB_DXEPCTL_EPENA;
        usbotgfs_write32(usbotgfs_doep_offset(ep, USB_DOEPCTL_OFFSET), reg);
        g_ep_state[ep][USBOTGFS_DIR_OUT_IDX] = USBOTG_FS_EP_STATE_IDLE;
    }

    return 0;
}

void usbotgfs_set_address(uint16_t addr)
{
    uint32_t dcfg;

    if (!g_initialized) {
        return;
    }

    dcfg = usbotgfs_read32(USB_DCFG_OFFSET);
    dcfg &= ~USB_DCFG_DAD_MASK;
    dcfg |= (((uint32_t)addr & 0x7FUL) << USB_DCFG_DAD_SHIFT);
    usbotgfs_write32(USB_DCFG_OFFSET, dcfg);
}

usbotgfs_ep_state_t usbotgfs_get_ep_state(uint8_t epnum, usbotgfs_ep_dir_t dir)
{
    const int idx = usbotgfs_dir_to_index(dir);

    if (epnum >= USBOTGFS_NUM_EPS || idx < 0) {
        return USBOTG_FS_EP_STATE_INVALID;
    }

    return g_ep_state[epnum][idx];
}

uint16_t usbotgfs_get_ep_mpsize(usbotgfs_ep_type_t type)
{
    switch (type) {
        case USBOTG_FS_EP_TYPE_ISOCHRONOUS:
            return 1024U;
        case USBOTG_FS_EP_TYPE_CONTROL:
        case USBOTG_FS_EP_TYPE_BULK:
        case USBOTG_FS_EP_TYPE_INT:
        default:
            return 64U;
    }
}

usbotgfs_port_speed_t usbotgfs_get_speed(void)
{
    if (g_initialized) {
        usbotgfs_update_port_speed();
    }
    return g_port_speed;
}
