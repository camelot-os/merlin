// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 H2Lab Development Team

#ifndef ILI2130_DRIVER_H
#define ILI2130_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct touch_informations {
    uint16_t x;
    uint16_t y;
    uint8_t pressure;
    uint8_t touch_id;
};

int ilitech_2130_i2c_init(enum i2c_speeds speed, enum i2c_address_mode mode);
int ilitech_2130_isr(int IRQn);
int ilitech_2130_sleep(void);
int ilitech_2130_wake(void);
int ilitech_2130_get_last_touch_info(struct touch_informations *touch_info);

#endif /* ILI2130_DRIVER_H */
