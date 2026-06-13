// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_CAN_H
#define MERLIN_CAN_H

#include <types.h>

/**
 * @brief CAN identifier format.
 */
enum can_id_type {
    CAN_ID_STANDARD,
    CAN_ID_EXTENDED,
};

/**
 * @brief CAN frame format.
 */
enum can_frame_format {
    CAN_FRAME_CLASSIC,
    CAN_FRAME_FD,
};

/**
 * @brief CAN controller runtime mode.
 */
enum can_mode {
    CAN_MODE_NORMAL,
    CAN_MODE_LOOPBACK_INTERNAL,
    CAN_MODE_LISTEN_ONLY,
    CAN_MODE_LISTEN_LOOPBACK,
};

/**
 * @brief Generic CAN frame.
 */
struct can_frame {
    uint32_t id;
    enum can_id_type id_type;
    enum can_frame_format format;
    bool rtr;
    bool brs;
    uint8_t dlc;
    uint8_t data[64];
};

/**
 * @brief Generic CAN/FDCAN controller configuration.
 */
struct can_config {
    enum can_mode mode;
    uint32_t nominal_bitrate;
    uint32_t data_bitrate;
    bool fd_enable;
    bool brs_enable;
};

#endif /*!MERLIN_CAN_H*/
