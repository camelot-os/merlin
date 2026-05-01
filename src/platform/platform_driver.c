// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#include <types.h>
#include <handle.h>
#include <uapi.h>
#include <merlin/platform/driver.h>
#include <merlin/helpers.h>
#include "../buses/i2c/i2c.h"
#include "../buses/usb/usb.h"

/* Note that Sentry tasks are monothreaded and thus do not require locks or concurrency check ath Merlin level */
static struct platform_device_driver *registered_drivers[CONFIG_MAX_REGISTERED_DRIVERS];
static size_t num_registered_drivers = 0;



/* private platform API that allow DTS content manipulation
 * The dts structure is forged build time in another local file
 * These functions are implemented in the devices-dt.c.in file
 */
Status merlin_platform_dts_get_baseaddr(struct platform_device_driver *drv, size_t *baseaddr)
{
    Status res = STATUS_INVALID;
    if (unlikely(drv == NULL || baseaddr == NULL)) {
        goto end;
    }
    if (drv->devinfo == NULL) {
        res = STATUS_NO_ENTITY;
        goto end;
    }
    *baseaddr = (size_t)drv->devinfo->baseaddr;
    res = STATUS_OK;
end:
    return res;
}

Status merlin_platform_dts_get_size(struct platform_device_driver *drv, size_t *size)
{
    Status res = STATUS_INVALID;
    if (unlikely(drv == NULL || size == NULL)) {
        goto end;
    }
    if (drv->devinfo == NULL) {
        res = STATUS_NO_ENTITY;
        goto end;
    }
    *size = (size_t)drv->devinfo->size;
    res = STATUS_OK;
end:
    return res;
}

Status merlin_platform_dts_get_irqs(struct platform_device_driver *drv, uint8_t num_irqs, it_info_t *irqs)
{
    Status res = STATUS_INVALID;
    if (unlikely(drv == NULL || irqs == NULL)) {
        goto end;
    }
    if (drv->devinfo == NULL) {
        res = STATUS_NO_ENTITY;
        goto end;
    }
    if (unlikely(num_irqs < drv->devinfo->num_interrupt)) {
        res = STATUS_INVALID;
        goto end;
    }
    for (size_t i = 0; i < drv->devinfo->num_interrupt; i++) {
        irqs[i].it_controler = drv->devinfo->its[i].it_controler;
        irqs[i].it_num = drv->devinfo->its[i].it_num;
    }
    if (num_irqs > drv->devinfo->num_interrupt) {
        for (size_t i = drv->devinfo->num_interrupt; i < num_irqs; i++) {
            irqs[i].it_controler = 0;
            irqs[i].it_num = 0;
        }
    }
    res = STATUS_OK;
end:
    return res;
}


static Status merlin_platform_dts_get_driver(uint16_t IRQn, struct platform_device_driver **drv)
{
    Status res = STATUS_NO_ENTITY;
    if (unlikely(drv == NULL)) {
        res = STATUS_INVALID;
        goto end;
    }
    for (size_t i = 0; i < num_registered_drivers; i++) {
        if (registered_drivers[i]->devinfo != NULL) {
            for (size_t j = 0; j < registered_drivers[i]->devinfo->num_interrupt; j++) {
                if (registered_drivers[i]->devinfo->its[j].it_num == IRQn) {
                    *drv = registered_drivers[i];
                    res = STATUS_OK;
                    goto end;
                }
            }
        }
    }
end:
    return res;
}

Status merlin_platform_get_driver_from_irq(uint32_t IRQn, struct platform_device_driver **drv)
{
    Status res = STATUS_NO_ENTITY;
    if (unlikely(drv == NULL)) {
        res = STATUS_INVALID;
        goto end;
    }
    for (size_t i = 0; i < num_registered_drivers; i++) {
        for (size_t j = 0; j < registered_drivers[i]->devinfo->num_interrupt; j++) {
            if (registered_drivers[i]->devinfo->its[j].it_num == IRQn) {
                *drv = registered_drivers[i];
                res = STATUS_OK;
                goto end;
            }
        }
    }
end:
    return res;
}

static Status merlin_platform_get_from_handle(devh_t handle, struct platform_device_driver **drv)
{
    Status res = STATUS_NO_ENTITY;
    if (unlikely(drv == NULL)) {
        res = STATUS_INVALID;
        goto end;
    }
    for (size_t i = 0; i < num_registered_drivers; i++) {
        if (registered_drivers[i]->devh == handle) {
            *drv = registered_drivers[i];
            res = STATUS_OK;
            goto end;
        }
    }
end:
    return res;
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
    struct platform_device_driver *platform_device_driver;
    if (unlikely(merlin_platform_get_driver_from_irq(IRQn, &platform_device_driver) != STATUS_OK)) {
        return STATUS_INVALID;
    }
    struct platform_device_driver *self;
    if (unlikely(merlin_platform_get_from_handle(platform_device_driver->devh, &self) != STATUS_OK)) {
        return STATUS_INVALID;
    }
    /* calling device driver interrupt service routine */
    return self->platform_fops.isr(self, IRQn);
}


size_t merlin_platform_get_baseaddr(struct platform_device_driver *self, size_t *baseaddr)
{
    Status res = STATUS_INVALID;
    if (unlikely(self == NULL || baseaddr == NULL)) {
        goto end;
    }
    if (self->devinfo == NULL) {
        res = STATUS_NO_ENTITY;
        goto end;
    }
    *baseaddr = (size_t)self->devinfo->baseaddr;
    res = STATUS_OK;
end:
    return res;
}

Status merlin_platform_driver_register(struct platform_device_driver *driver, uint32_t label)
{
    const devinfo_t *devinfo;
    if (unlikely(driver == NULL)) {
        return STATUS_INVALID;
    }
    if (unlikely(num_registered_drivers >= CONFIG_MAX_REGISTERED_DRIVERS)) {
        return STATUS_BUSY;
    }
    switch (driver->type) {
        case DEVICE_TYPE_I2C:
            if (unlikely(merlin_platform_dts_i2c_get_devinfo(label, &devinfo) != STATUS_OK)) {
                return STATUS_INVALID;
            }
            break;
        case DEVICE_TYPE_USB:
            if (unlikely(merlin_platform_dts_usb_get_devinfo(label, &devinfo) != STATUS_OK)) {
                return STATUS_INVALID;
            }
            break;
        default:
            return STATUS_INVALID;
    }
    driver->label = label;
    /* FIXME: Sentry kernel device list must be fixed to use sentry,label instead of loop index for identifying devices,
     * This is the way other ressources (such as SHM or DMA) work, and is required for consistent device management at userspace level */

     /* get device handle based on its label */
    if (unlikely(__sys_get_device_handle(label) != STATUS_OK)) {
        return STATUS_INVALID;
    }
    copy_from_kernel((uint8_t*)&driver->devh, sizeof(devh_t));

    registered_drivers[num_registered_drivers] = driver;
    num_registered_drivers++;
    driver->devinfo = devinfo;

    return STATUS_OK;
}

Status merlin_platform_driver_configure_gpio(struct platform_device_driver *self)
{
    Status status = STATUS_INVALID;
    const devinfo_t *devinfo = NULL;
    if (unlikely(self == NULL)) {
        goto end;
    }
    devinfo = self->devinfo;
    if (unlikely(devinfo == NULL)) {
         goto end;
    }
    for (size_t i = 0; i < devinfo->num_ios; i++) {
        status = __sys_gpio_configure(self->devh, i);
        if (unlikely(status != STATUS_OK)) {
            goto end;
        }
    }
end:
    return status;
}

Status merlin_platform_acknowledge_irq(struct platform_device_driver *self, uint32_t IRQn)
{
    Status res = STATUS_NO_ENTITY;
    for (size_t i = 0; i < self->devinfo->num_interrupt; i++) {
        if (self->devinfo->its[i].it_num == IRQn) {
            res = __sys_irq_acknowledge(self->devinfo->its[i].it_num);
        }
    }
    return res;
}
