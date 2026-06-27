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

int ilitech_2130_probe(uint32_t buslabel);
int ilitech_2130_init(uint32_t buslabel);
int ilitech_2130_isr(void *self, uint32_t IRQn);
int ilitech_2130_sleep(uint32_t buslabel);
int ilitech_2130_wake(uint32_t buslabel);
int ilitech_2130_get_last_touch_info(uint32_t buslabel, struct touch_informations *touch_info);

#endif /* ILI2130_DRIVER_H */
