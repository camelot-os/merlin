#include <types.h>
#include <handle.h>
#include <uapi.h>
#include <merlin/platform/driver.h>
#include "dts.h"
#include "../helper.h"

static struct platform_device_driver *merlin_platform_get_from_handle(devh_t handle)
{
    /* define a merlin-level platform drivers listing context to search in */
    return NULL;
}

/* Map given device handle into the task memory */
Status merlin_platform_driver_map(struct platform_device_driver *self)
{
    if (unlikely(self == NULL)) {
        return STATUS_INVALID;
    }
    return __sys_map_dev(self->devh);
}

/* Unmap given device handle from the task memory */
Status merlin_platform_driver_unmap(struct platform_device_driver *self)
{
    if (unlikely(self == NULL)) {
        return STATUS_INVALID;
    }
    return __sys_unmap_dev(self->devh);
}

/* resolve driver context in the registered driver(s) for given IRQn using DTS backend,
 * and call the fops->isr routine for it.
 * Minimalist model, to auto-dispatch IRQ events received from sys_get_event()
 */
Status merlin_platform_driver_irq_displatch(uint32_t IRQn)
{
    struct platform_device_driver platform_device_driver;
    if (unlikely(merlin_platform_dts_get_driver(IRQn, &platform_device_driver) != STATUS_OK)) {
        return STATUS_INVALID;
    }
    struct platform_device_driver *self = merlin_platform_get_from_handle(platform_device_driver.devh);
    return self->platform_fops.isr(self, IRQn);
}


size_t merlin_platform_get_baseaddr(struct platform_device_driver *self)
{
    return merlin_platform_dts_get_baseaddr(self);
}
