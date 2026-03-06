#ifndef MERLIN_DTS_H
#define MERLIN_DTS_H

#include <types.h>
#include <merlin/platform/driver.h>

/* private platform API that allow DTS content manipulation
 * The dts structure is forged build time in another local file
 * These functions are implemented in the devices-dt.c.in file
 */
Status merlin_platform_dts_get_baseaddr(struct platform_device_driver *drv);
Status merlin_platform_dts_get_len(struct platform_device_driver *drv);
Status merlin_platform_dts_map(struct platform_device_driver *drv);
Status merlin_platform_dts_unmap(struct platform_device_driver *drv);
Status merlin_platform_dts_get_irqs(struct platform_device_driver *drv);
Status merlin_platform_dts_get_driver(uint32_t IRQn, struct platform_device_driver *drv);

#endif/*!MERLIN_DTS_H*/
