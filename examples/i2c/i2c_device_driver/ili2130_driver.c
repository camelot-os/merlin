// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 H2Lab Development Team

#include <string.h>
#include <i2c_bus_driver.h>
#include <ili2130_driver.h>
#include <merlin/platform/driver.h>
#include <merlin/buses/i2c.h>
#include <merlin/io.h>
#include <merlin/helpers.h>
#include <uapi.h>


#define ILI2130_I2C_ADDR 0x41

#define ILI2130_REG_CHIP_ID 0x00
#define ILI2130_CHIP_ID_VALUE 0x2130
#define ILI2130_REG_FW_VER 0x01
#define ILI2130_REG_GESTURE 0x10
#define ILI2130_GESTURE_NONE 0x00
#define ILI2130_GESTURE_SWIPE_UP 0x01
#define ILI2130_GESTURE_SWIPE_DOWN 0x02
#define ILI2130_GESTURE_SWIPE_LEFT 0x03
#define ILI2130_GESTURE_SWIPE_RIGHT 0x04
#define ILI2130_GESTURE_TAP 0x05
#define ILI2130_GESTURE_DOUBLE_TAP 0x06
#define ILI2130_GESTURE_LONG_PRESS 0x07
#define ILI2130_REG_TOUCH_DATA 0x20
#define ILI2130_MAX_TOUCH_POINTS 5
#define ILI2130_TOUCH_POINT_DATA_SIZE 6 /* x (2 bytes), y (2 bytes), pressure (1 byte), touch ID (1 byte) */
#define ILI2130_REG_INT_STATUS 0x30
#define ILI2130_INT_STATUS_TOUCH 0x01
#define ILI2130_INT_STATUS_GESTURE 0x02
#define ILI2130_REG_INT_ENABLE 0x31
#define ILI2130_INT_ENABLE_TOUCH 0x01
#define ILI2130_INT_ENABLE_GESTURE 0x02
#define ILI2130_REG_INT_CLEAR 0x32
#define ILI2130_REG_POWER_CTRL 0x40
#define ILI2130_POWER_CTRL_SLEEP 0x01
#define ILI2130_POWER_CTRL_WAKE 0x00

#define ILI2130_GPIO_IO_RSTN 0U
#define ILI2130_GPIO_IO_INTN_OUT 2U
#define ILI2130_RESET_ASSERT_DELAY_MS 20U
#define ILI2130_RESET_RELEASE_DELAY_MS 100U


bool touch_event_received = false;
struct touch_informations last_touch_info = {0};

struct platform_device_driver ili2130_driver = {
    .devh = 0, /* to be updated by driver probe */
    .label = 0x0UL, /* to be updated by driver probe */
    .devinfo = NULL, /* to be updated by driver probe */
    .name = "IliTech 2130 touch controller driver",
    .compatible = "gpio,ili2130",
    .platform_fops = {
        .isr = ilitech_2130_isr,
    },
    .type = DEVICE_TYPE_GPIO,
};

static int ilitech_2130_sleep_ms(uint32_t duration_ms)
{
    SleepDuration duration;

    duration.tag = SLEEP_DURATION_ARBITRARY_MS;
    duration.arbitrary_ms = duration_ms;

    if (__sys_sleep(duration, SLEEP_MODE_SHALLOW) != STATUS_OK) {
        return -1;
    }

    return 0;
}

static int ilitech_2130_reset_with_configured_pinmux(struct platform_device_driver *self)
{
    if (unlikely(self == NULL)) {
        return -1;
    }

    if (self->devh == 0) {
        return -1;
    }

    if (self->devinfo != NULL) {
        if (merlin_platform_driver_configure_gpio(self) != STATUS_OK) {
            return -1;
        }
    } else {
        if (__sys_gpio_configure(self->devh, ILI2130_GPIO_IO_RSTN) != STATUS_OK) {
            return -1;
        }
        if (__sys_gpio_configure(self->devh, ILI2130_GPIO_IO_INTN_OUT) != STATUS_OK) {
            return -1;
        }
    }

    if (__sys_gpio_set(self->devh, ILI2130_GPIO_IO_INTN_OUT, true) != STATUS_OK) {
        return -1;
    }

    if (__sys_gpio_set(self->devh, ILI2130_GPIO_IO_RSTN, false) != STATUS_OK) {
        return -1;
    }

    if (ilitech_2130_sleep_ms(ILI2130_RESET_ASSERT_DELAY_MS) != 0) {
        return -1;
    }

    if (__sys_gpio_set(self->devh, ILI2130_GPIO_IO_RSTN, true) != STATUS_OK) {
        return -1;
    }

    if (ilitech_2130_sleep_ms(ILI2130_RESET_RELEASE_DELAY_MS) != 0) {
        return -1;
    }

    return 0;
}

int ilitech_2130_probe(void)
{
    int res = -1;
    /* first register against merlin, that do probe against the task DTS */
    if (unlikely(merlin_platform_driver_register(&ili2130_driver, 0x101) != STATUS_OK)) {
        goto end;
    }

    if (ilitech_2130_reset_with_configured_pinmux(&ili2130_driver) != 0) {
        goto end;
    }
    /*
     * here we consider that the I2C bus on which the touch controller is connected is already initialized and registered
     * This means that the I2C bus controller driver must have been probed and initialized before, and thus is mapped in the task.
     */
    /* identify the device first */
    uint8_t chip_id[2];
    if (i2c_bus_read7(ILI2130_I2C_ADDR, ILI2130_REG_CHIP_ID, chip_id, sizeof(chip_id)) != 0) {
        goto end;
    }
    uint16_t chip_id_value = (chip_id[0] << 8) | chip_id[1];
    if (chip_id_value != ILI2130_CHIP_ID_VALUE) {
        goto end;
    }

    res = 0;
end:
    return res;
}

int ilitech_2130_init(void)
{
    int res = -1;

    if (ilitech_2130_reset_with_configured_pinmux(&ili2130_driver) != 0) {
        goto end;
    }

    /* here we consider that the I2C bus on which the touch controller is connected is already initialized and registered */
    /* identify the device first */
    uint8_t chip_id[2];
    if (i2c_bus_read7(ILI2130_I2C_ADDR, ILI2130_REG_CHIP_ID, chip_id, sizeof(chip_id)) != 0) {
        goto end;
    }
    uint16_t chip_id_value = (chip_id[0] << 8) | chip_id[1];
    if (chip_id_value != ILI2130_CHIP_ID_VALUE) {
        goto end;
    }

    /* functional init sequence: touch only, polling mode, no interrupt */

    /* ensure controller is awake */
    uint8_t power_ctrl = ILI2130_POWER_CTRL_WAKE;
    if (i2c_bus_write7(ILI2130_I2C_ADDR, ILI2130_REG_POWER_CTRL, &power_ctrl, 1) != 0) {
        goto end;
    }

    /* disable gesture engine */
    uint8_t gesture_mode = ILI2130_GESTURE_NONE;
    if (i2c_bus_write7(ILI2130_I2C_ADDR, ILI2130_REG_GESTURE, &gesture_mode, 1) != 0) {
        goto end;
    }

    /* disable all interrupts: polling mode */
    uint8_t int_enable = 0x00;
    if (i2c_bus_write7(ILI2130_I2C_ADDR, ILI2130_REG_INT_ENABLE, &int_enable, 1) != 0) {
        goto end;
    }

    /* clear pending touch/gesture status before entering polling loop */
    uint8_t int_clear = ILI2130_INT_STATUS_TOUCH | ILI2130_INT_STATUS_GESTURE;
    if (i2c_bus_write7(ILI2130_I2C_ADDR, ILI2130_REG_INT_CLEAR, &int_clear, 1) != 0) {
        goto end;
    }

    touch_event_received = false;
    memset(&last_touch_info, 0, sizeof(last_touch_info));

    res = 0;
end:
    return res;
}

int ilitech_2130_poll_touch_info(struct touch_informations *touch_info)
{
    int res = -1;
    /* react to touch interrupt, and get back corrdinates in one-touch mode for simplicity */
    uint8_t int_status;
    if (i2c_bus_read7(ILI2130_I2C_ADDR, ILI2130_REG_INT_STATUS, &int_status, 1) != 0) {
        goto end;
    }
    if (int_status & ILI2130_INT_STATUS_TOUCH) {
        uint8_t touch_data[ILI2130_TOUCH_POINT_DATA_SIZE];
        if (i2c_bus_read7(ILI2130_I2C_ADDR, ILI2130_REG_TOUCH_DATA, touch_data, sizeof(touch_data)) != 0) {
            goto end;
        }
        uint16_t x = (touch_data[0] << 8) | touch_data[1];
        uint16_t y = (touch_data[2] << 8) | touch_data[3];
        uint8_t pressure = touch_data[4];
        uint8_t touch_id = touch_data[5];
        last_touch_info.x = x;
        last_touch_info.y = y;
        last_touch_info.pressure = pressure;
        last_touch_info.touch_id = touch_id;
        touch_event_received = true;
    }
    if (int_status & ILI2130_INT_STATUS_GESTURE) {
        /* in that example, we do not handle gesture events, but the same principle can be applied to read gesture data and react accordingly */
    }
    res = 0;
end:
    return res;
}

int ilitech_2130_sleep(void)
{
    int res = -1;
    /* put the touch controller in sleep mode */
    uint8_t power_ctrl = ILI2130_POWER_CTRL_SLEEP;
    if (i2c_bus_write7(ILI2130_I2C_ADDR, ILI2130_REG_POWER_CTRL, &power_ctrl, 1) != 0) {
        goto end;
    }
    res = 0;
end:
    return res;
}

int ilitech_2130_wake(void)
{
    int res = -1;
    /* wake up the touch controller */
    uint8_t power_ctrl = ILI2130_POWER_CTRL_WAKE;
    if (i2c_bus_write7(ILI2130_I2C_ADDR, ILI2130_REG_POWER_CTRL, &power_ctrl, 1) != 0) {
        goto end;
    }
    res = 0;
end:
    return res;
}

int ilitech_2130_get_last_touch_info(struct touch_informations *touch_info)
{
    int res = -1;
    if ((unlikely(touch_info == NULL))) {
        goto end;
    }
    if (!touch_event_received) {
        goto end;
    }
    memcpy(touch_info, &last_touch_info, sizeof(struct touch_informations));
    res = 0;
end:
    return res;
}
