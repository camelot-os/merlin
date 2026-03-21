/*
 * Copyright (c) 2026 H2Lab Development Team
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef ST_OTGFS_DRIVER_H
#define ST_OTGFS_DRIVER_H


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <merlin/buses/usb.h>


typedef int (*usbotgfs_ioep_handler_t)(devh_t devh, size_t size, uint8_t ep);

/*
 * Device Endpoint identifiers
 */
typedef enum {
    USBOTG_FS_EP0 = 0,
    USBOTG_FS_EP1 = 1,
    USBOTG_FS_EP2 = 2,
    USBOTG_FS_EP3 = 3,
    USBOTG_FS_EP4 = 4,
    USBOTG_FS_EP5 = 5,
    USBOTG_FS_EP6 = 6,
    USBOTG_FS_EP7 = 7,
} usbotgfs_ep_nb_t;

/*
 * Max packet size in EP0 is a specific field.
 * Here are the supported size
 */
typedef enum {
    USBOTG_FS_EP0_MPSIZE_64BYTES = 0,
    USBOTG_FS_EP0_MPSIZE_32BYTES = 1,
    USBOTG_FS_EP0_MPSIZE_16BYTES = 2,
    USBOTG_FS_EP0_MPSIZE_8BYTES  = 3,
} usbotgfs_ep0_mpsize_t;


/*
 * Other EPs support various sizes for their
 * max packet size. Although, we limit these size to
 * various standard sizes.
 */
typedef enum {
    USBOTG_FS_EPx_MPSIZE_64BYTES = 64,
    USBOTG_FS_EPx_MPSIZE_128BYTES = 128,
    USBOTG_FS_EPx_MPSIZE_512BYTES = 512,
    USBOTG_FS_EPx_MPSIZE_1024BYTES  = 1024,
} usbotgfs_epx_mpsize_t;

typedef enum {
    USB_FS_DXEPCTL_SD0PID_SEVNFRM  = 0,
    USB_FS_DXEPCTL_SD1PID_SODDFRM  = 1
} usbotgfs_ep_toggle_t;

/*
 * USB standard EP type
 */
typedef enum {
    USBOTG_FS_EP_TYPE_CONTROL     = 0,
    USBOTG_FS_EP_TYPE_ISOCHRONOUS = 1,
    USBOTG_FS_EP_TYPE_BULK        = 2,
    USBOTG_FS_EP_TYPE_INT         = 3,
} usbotgfs_ep_type_t;

/*
 * Global device state, depending on currently send/received data.
 * This flags are (mostly) set by rxflvl handler and can be read back
 * at oepint/iepint time to be informed of which data type is currently
 * waiting for treatment (reception case) or has been sent (transmission
 * case). The driver reset the current flag to IDLE automatically when the
 * data has be treated in iepint/oepint end of function.
 */
typedef enum {
    USBOTG_FS_EP_STATE_IDLE  = 0,
    USBOTG_FS_EP_STATE_SETUP_WIP = 1,
    USBOTG_FS_EP_STATE_SETUP = 2,
    USBOTG_FS_EP_STATE_STATUS = 3,
    USBOTG_FS_EP_STATE_STALL = 4,
    USBOTG_FS_EP_STATE_DATA_IN_WIP = 5,
    USBOTG_FS_EP_STATE_DATA_IN = 6,
    USBOTG_FS_EP_STATE_DATA_OUT_WIP = 7,
    USBOTG_FS_EP_STATE_DATA_OUT = 8,
    USBOTG_FS_EP_STATE_INVALID = 9,
} usbotgfs_ep_state_t;

typedef enum {
    USBOTG_FS_EP_DIR_IN = 0,
    USBOTG_FS_EP_DIR_OUT = 1,
    USBOTG_FS_EP_DIR_BOTH = 2,
} usbotgfs_ep_dir_t;

typedef enum {
    USBOTG_FS_PORT_SPEED_LOW = 0,
    USBOTG_FS_PORT_SPEED_FULL = 1,
    USBOTG_FS_PORT_SPEED_HIGH = 2,
    USBOTG_FS_PORT_SPEED_UNKNOWN = 3,
} usbotgfs_port_speed_t;


/*********************************************************************************
 * About handlers
 *
 * The Control plane must declare some handlers for various events (see usbotgfs_handlers.c
 * for more informations). These handlers are called on these events in order to execute
 * control level (or potentially upper level(s)) programs. They can use the USB OTG FS
 * driver API during their execution.
 *
 * These handers are implemented at upper layer level, in the USB control plane stack,
 * and are not part of the driver implementation. Although, they must be declared in the
 * driver header file, as they are mandatory for the USB stack to work, and they are called by the
 * driver on the corresponding events.
 *
 * Control level handlers are linked directly through their prototype definition here.
 *
 * We prefer to use prototype and link time symbol resolution instead of callbacks as:
 *   1. The USB control plane is not an hotpluggable element
 *   2. callbacks have security impacts, as they can be corrupted, generating arbitrary
 *      code execution
 *
 *  WARNING: as we use prototypes (and not callbacks), these functions *must* exists at
 *  link time, for symbol resolution.
 *  It has been decided that the driver doesn't hold weak symbols for these functions,
 *  as their absence would make the USB stack unfonctional.
 *  If one of these function is not set in the control plane (or in any element of the
 *  application to be linked) it would generate a link time error, corresponding to a
 *  missing symbol.
 *
 */

int usbctrl_handle_earlysuspend(uint32_t dev_id);
int usbctrl_handle_reset(uint32_t dev_id);
int usbctrl_handle_usbsuspend(uint32_t dev_id);
int usbctrl_handle_inepevent(uint32_t dev_id, uint32_t size, uint8_t ep);
int usbctrl_handle_outepevent(uint32_t dev_id, uint32_t size, uint8_t ep);
int usbctrl_handle_wakeup(uint32_t dev_id);


int usbotgfs_probe(uint32_t label);

int usbotgfs_init(enum usb_otg_mode mode, enum usb_maximum_speed max_speed);

/*
 * Sending data (whatever data type is (i.e. status on control pipe or data on
 * data (Bulk, IT or isochronous) pipe)
 * This is not a syncrhonous request, i.e. data are stored into the USB OTG FS
 * interanal FIFO, waiting for bus transmission. When data are fully transmitted,
 * a iepint (device mode) or oepint (host mode) is triggered to inform the upper
 * layer that the content has been sent. Although, it is possible to push some
 * other data in the internal FIFO if needed, while this FIFO is not full
 * (check for this function return value)
 *
 * @src the RAM FIFO from which the data are read
 * @size the amount of data bytes to send
 * @ep the endpoint on which the data are to be sent
 *
 * @return 0 if data has been correctly transmitted into the internal
 * core FIFO, or a negative error code if the internal core FIFO for the given EP is full
 */
int usbotgfs_send_data(uint8_t *src, uint32_t size, uint8_t ep);

/*
 * Configure for receiving data. Receiving data is a triggering event, not a direct call.
 * As a consequence, the upper layers have to specify the amount of data requested for
 * the next USB transaction on the given OUT (device mode) or IN (host mode) enpoint.
 *
 * @dst is the destination buffer that will be used to hold  @size amount of data bytes
 * @size is the amount of data bytes to load before await the upper stack
 * @ep is the active endpoint on which this action is done
 *
 * On data reception:
 * - if there is enough data loaded in the output buffer, the upper stack is awoken
 * - If not, data is silently stored in FIFO RAM (targetted by dst), and the driver waits
 *   for the next content while 'size' amount of data is not reached
 *
 * @return 0 if setup is ok, or a negative error code if an error occurs     (INVSTATE
 * for invalid enpoint type, INVPARAM if dst is NULL or size invalid)
 */
int usbotgfs_set_recv_fifo(uint8_t *dst, uint32_t size, uint8_t ep);

/*
 * Send a special zero-length packet on EP ep
 */
int usbotgfs_send_zlp(uint8_t ep);

/*
 * Activate the configuration global stall mode. If any EP has its stall mode configured,
 * it can override the global stall mode
 * INFO: stall mode for Control and data EP don't have the same meaning. See datasheet,
 * chap 35.13.7
 */
int usbotgfs_global_stall(void);

/*
 * Clear the global stall mode.
 */
int usbotgfs_global_stall_clear(void);

/*
 * Set the STALL mode for the given EP
 */
int usbotgfs_endpoint_stall(uint8_t ep_id, usbotgfs_ep_dir_t dir);

/*
 * Clear the STALL mode for the given EP
 */
int usbotgfs_endpoint_stall_clear(uint8_t ep, usbotgfs_ep_dir_t dir);

int usbotgfs_endpoint_set_nak(uint8_t ep_id, usbotgfs_ep_dir_t dir);

int usbotgfs_endpoint_clear_nak(uint8_t ep_id, usbotgfs_ep_dir_t dir);

/*
 * Activate the given EP (for e.g. to transmit data)
 */
int usbotgfs_configure_endpoint(uint8_t               ep,
                                usbotgfs_ep_type_t    type,
                                usbotgfs_ep_dir_t     dir,
                                usbotgfs_epx_mpsize_t mpsize,
                                usbotgfs_ep_toggle_t  dtoggle,
                                usbotgfs_ioep_handler_t handler);

/*
 * Deactivate the given EP (Its configuration is keeped, the EP is *not* deconfigured)
 */
int usbotgfs_deconfigure_endpoint(uint8_t ep);

/*
 * Configure the given EP in order to be ready to work
 */
int usbotgfs_activate_endpoint(uint8_t               id,
                               usbotgfs_ep_dir_t     dir);

/*
 * Deconfigure the given EP. The EP is no more usable after this call. A new configuration
 * of the EP must be done before reuse it.
 * This function is typically used on SetConfiguration Requests schedule, or on
 * Reset frame reception to reconfigure the Core in a known clear state.
 */
int usbotgfs_deactivate_endpoint(uint8_t ep,
                                 usbotgfs_ep_dir_t     dir);


/**
 * usb_driver_set_address - Set the address of the device
 * @addr: Device's address
 */
void usbotgfs_set_address(uint16_t addr);

usbotgfs_ep_state_t usbotgfs_get_ep_state(uint8_t epnum, usbotgfs_ep_dir_t dir);

uint16_t usbotgfs_get_ep_mpsize(usbotgfs_ep_type_t type);

usbotgfs_port_speed_t usbotgfs_get_speed(void);

#endif /*!ST_OTGFS_DRIVER_H */
