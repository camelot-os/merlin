// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#include <types.h>
#include <merlin/platform/driver.h>
#include "spi_dt.h"
#include "spi.h"



Status merlin_platform_dts_spi_get_devinfo(uint32_t devlabel, const devinfo_t **devinfo)
{
    Status status = STATUS_NO_ENTITY;
#if DEV_ID_SPI_MAX > 0
    for (size_t i = 0; i < DEV_ID_SPI_MAX; i++) {
        if (spi_devices[i].devinfo.id == devlabel) {
            *devinfo = &spi_devices[i].devinfo;
            status = STATUS_OK;
        }
    }
#endif
    return status;
}

/**
 * @brief get the SPI bus controller pre-scaler divider value for the desired SPI speed mode
 *
 * This function is used to retrieve the pre-scaler divider value that need to be set in
 * the SPI bus controller. Note that this value is the mathematical divider value, not the
 * actual value to be set in the register, which can be different depending on the SPI bus controller.
 * This function is typically used in the SPI bus controller initialization routine, to configure the
 * bus controller to operate at the desired speed.
 */
Status merlin_spi_get_precaler_div(struct platform_device_driver *drv, uint32_t *precaler_div)
{
    Status status = STATUS_INVALID;
    if (unlikely(drv == NULL || precaler_div == NULL)) {
        return STATUS_INVALID;
    }
#if DEV_ID_SPI_MAX > 0
    for (size_t i = 0; i < DEV_ID_SPI_MAX; i++) {
        if (drv->label == spi_devices[i].devinfo.id) {
            /* Note that the actual pre-scaler divider value to be set in the register can be different depending on the SPI bus controller, and thus need to be calculated in the driver implementation based on the mathematical divider value */
            *precaler_div = spi_devices[i].bus_input_clock_freq;
            status = STATUS_OK;
            goto end;
        }
    }
#endif
end:
    return status;
}
