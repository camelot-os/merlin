#include <i2c_dt.h>

Status merlin_platform_dts_i2c_get_baseaddr(struct platform_device_driver *drv);
Status merlin_platform_dts_i2c_get_len(struct platform_device_driver *drv);
Status merlin_platform_dts_i2c_map(struct platform_device_driver *drv);
Status merlin_platform_dts_i2c_unmap(struct platform_device_driver *drv);
Status merlin_platform_dts_i2c_get_irqs(struct platform_device_driver *drv);
Status merlin_platform_dts_i2c_get_driver(uint32_t IRQn, struct platform_device_driver *drv);
