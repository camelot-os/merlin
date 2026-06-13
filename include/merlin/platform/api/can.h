// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_PLATFORM_API_CAN_H
#define MERLIN_PLATFORM_API_CAN_H

#include <inttypes.h>

#include <merlin/buses/can.h>
#include <merlin/platform/api/generic.h>
#include <merlin/platform/driver.h>

/**
 * @brief Probe a CAN controller instance from its DTS label.
 */
drv_status_t can_probe(uint32_t label);

/**
 * @brief Initialize a CAN controller instance.
 */
drv_status_t can_init(uint32_t label, const struct can_config *cfg);

/**
 * @brief Send one CAN frame.
 */
drv_status_t can_send(uint32_t label, const struct can_frame *frame);

/**
 * @brief Receive one CAN frame.
 */
drv_status_t can_recv(uint32_t label, struct can_frame *frame);

/**
 * @brief Release a CAN controller instance.
 */
drv_status_t can_release(uint32_t label);

#endif /* MERLIN_PLATFORM_API_CAN_H */
