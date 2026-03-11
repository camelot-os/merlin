// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 H2Lab Development Team

#include <string.h>
#include <i2c_bus_driver.h>
#include <ili2130_driver.h>
#include <merlin/platform/driver.h>
#include <merlin/buses/i2c.h>
#include <merlin/io.h>
#include <merlin/helpers.h>


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


bool touch_event_received = false;
struct touch_informations last_touch_info = {0};

int ilitech_2130_i2c_init(enum i2c_speeds speed, enum i2c_address_mode mode)
{
    int res = -1;
    /* in that example, we do not support dynamic address mode */
    if (mode != I2C_ADDRESS_7B) {
        goto end;
    }
    /* for simplicity, we do not support different speed modes in that example */
    if (speed != I2C_SPEED_SM_100K) {
        goto end;
    }
    /* here we consider that the I2C bus on which the touch controller is connected is already initialized and registered */
    /* configure touchscreen through i2c */
    uint8_t chip_id[2];
    if (i2c_bus_read7(ILI2130_I2C_ADDR, ILI2130_REG_CHIP_ID, chip_id, sizeof(chip_id)) != 0) {
        goto end;
    }
    uint16_t chip_id_value = (chip_id[0] << 8) | chip_id[1];
    if (chip_id_value != ILI2130_CHIP_ID_VALUE) {
        goto end;
    }
    /* enable touch and gesture interrupts */
    uint8_t int_enable = ILI2130_INT_ENABLE_TOUCH | ILI2130_INT_ENABLE_GESTURE;
    if (i2c_bus_write7(ILI2130_I2C_ADDR, ILI2130_REG_INT_ENABLE, &int_enable, 1) != 0) {
        goto end;
    }
    res = 0;
end:
    return res;
}

int ilitech_2130_isr(int IRQn)
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
        /* clear touch interrupt */
        uint8_t int_clear = ILI2130_INT_STATUS_TOUCH;
        i2c_bus_write7(ILI2130_I2C_ADDR, ILI2130_REG_INT_CLEAR, &int_clear, 1);
    }
    if (int_status & ILI2130_INT_STATUS_GESTURE) {
        /* in that example, we do not handle gesture events, but the same principle can be applied to read gesture data and react accordingly */
        /* clear gesture interrupt */
        uint8_t int_clear = ILI2130_INT_STATUS_GESTURE;
        i2c_bus_write7(ILI2130_I2C_ADDR, ILI2130_REG_INT_CLEAR, &int_clear, 1);
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
