// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_PRIV_CAN_H
#define MERLIN_PRIV_CAN_H

#include <device.h>
#include <inttypes.h>
#include <merlin/platform/driver.h>
#include <merlin/helpers.h>

Status merlin_platform_dts_can_get_devinfo(uint32_t devlabel, const devinfo_t **devinfo);

Status merlin_can_get_precaler_div(struct platform_device_driver *drv, uint32_t *precaler_div);

#endif /* MERLIN_PRIV_CAN_H */
