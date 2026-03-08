// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_PRIV_I2C_H
#define MERLIN_PRIV_I2C_H

#include <device.h>
#include <inttypes.h>
#include <merlin/platform/driver.h>

Status merlin_platform_dts_i2c_get_baseaddr(struct platform_device_driver *drv);

Status merlin_platform_dts_i2c_get_len(struct platform_device_driver *drv);

Status merlin_platform_dts_i2c_map(struct platform_device_driver *drv);

Status merlin_platform_dts_i2c_unmap(struct platform_device_driver *drv);

Status merlin_platform_dts_i2c_get_irqs(struct platform_device_driver *drv);

Status merlin_platform_dts_i2c_get_devinfo(uint32_t devlabel, const devinfo_t **devinfo);


#endif/* MERLIN_PRIV_I2C_H */
