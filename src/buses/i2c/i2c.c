#include "i2c_dt.h"

Status merlin_platform_dts_i2c_get_baseaddr(struct platform_device_driver *drv)
{
    return STATUS_OK;
}

Status merlin_platform_dts_i2c_get_len(struct platform_device_driver *drv)
{
    return STATUS_OK;
}
Status merlin_platform_dts_i2c_map(struct platform_device_driver *drv)
{
    return STATUS_OK;
}
Status merlin_platform_dts_i2c_unmap(struct platform_device_driver *drv)
{
    return STATUS_OK;
}
Status merlin_platform_dts_i2c_get_irqs(struct platform_device_driver *drv)
{
    return STATUS_OK;
}
Status merlin_platform_dts_i2c_get_devinfo(uint32_t devlabel, const devinfo_t **devinfo)
{
    Status status = STATUS_NOENT;
    for (size_t i = 0; i < DEV_ID_I2C_MAX; i++) {
        if (i2c_devices[i].id == devlabel) {
            *devinfo = i2c_devices[i];
            status = STATUS_OK;
        }
    }
    return status;
}
