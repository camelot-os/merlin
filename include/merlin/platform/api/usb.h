// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_PLATFORM_API_USB_H
#define MERLIN_PLATFORM_API_USB_H

#include <stddef.h>
#include <stdint.h>

#include <merlin/buses/usb.h>
#include <merlin/platform/api/generic.h>
#include <merlin/platform/driver.h>

typedef int (*usb_ioep_handler_t)(devh_t devh, size_t size, uint8_t ep);

drv_status_t usb_probe(uint32_t label);
drv_status_t usb_init(uint32_t label, enum usb_otg_mode mode, enum usb_maximum_speed max_speed);
drv_status_t usb_release(uint32_t label);

drv_status_t usb_send_data(uint32_t label, uint8_t *src, uint32_t size, uint8_t ep);
drv_status_t usb_set_recv_fifo(uint32_t label, uint8_t *dst, uint32_t size, uint8_t ep);
drv_status_t usb_send_zlp(uint32_t label, uint8_t ep);

drv_status_t usb_global_stall(uint32_t label);
drv_status_t usb_global_stall_clear(uint32_t label);
drv_status_t usb_endpoint_stall(uint32_t label, uint8_t ep_id, enum usb_endpoint_direction dir);
drv_status_t usb_endpoint_stall_clear(uint32_t label, uint8_t ep_id, enum usb_endpoint_direction dir);
drv_status_t usb_endpoint_set_nak(uint32_t label, uint8_t ep_id, enum usb_endpoint_direction dir);
drv_status_t usb_endpoint_clear_nak(uint32_t label, uint8_t ep_id, enum usb_endpoint_direction dir);

drv_status_t usb_configure_endpoint(uint32_t label,
					    uint8_t ep,
					    enum usb_endpoint_type type,
					    enum usb_endpoint_direction dir,
					    enum usb_endpoint_mpsize mpsize,
					    enum usb_endpoint_toggle dtoggle,
					    usb_ioep_handler_t handler);
drv_status_t usb_deconfigure_endpoint(uint32_t label, uint8_t ep);
drv_status_t usb_activate_endpoint(uint32_t label, uint8_t ep, enum usb_endpoint_direction dir);
drv_status_t usb_deactivate_endpoint(uint32_t label, uint8_t ep, enum usb_endpoint_direction dir);

drv_status_t usb_set_address(uint32_t label, uint16_t addr);
drv_status_t usb_get_ep_state(uint32_t label, uint8_t epnum, enum usb_endpoint_direction dir,
					 enum usb_endpoint_state *state);
drv_status_t usb_get_ep_mpsize(uint32_t label, enum usb_endpoint_type type, uint16_t *mpsize);
drv_status_t usb_get_speed(uint32_t label, enum usb_port_speed *speed);

#endif /*!MERLIN_PLATFORM_API_USB_H*/