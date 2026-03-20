// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_HELPERS_H
#define MERLIN_HELPERS_H

#ifndef unlikely
/**
 * @brief Mark a condition as unlikely to be true
 *
 * This implementation uses the GCC built-in function __builtin_expect
 * to provide a hint to the compiler that the condition is unlikely to be true.
 * This can help the compiler optimize the code for better performance in the
 * common case where the condition is false.
 * This macro modify the way the compiler generate code for the condition,
 * and should be used with care, only when the condition is really unlikely
 * to be true, like, for example, for error handling.
 *
 * @param x condition to evaluate
 * @return evaluated condition
 */
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

/**
 * @brief get the parent bus label for a given external device using its label
 *
 * This function is used to retrieve the label of the parent bus for a given external device,
 * based on the device label set in the device tree and lonely information known by the task.
 * This is typically used by external device drivers to get the parent bus label, which is needed
 * to execute transactions with the device through the bus driver API.
 *
 * @param drv pointer to the platform_device_driver structure of the external device, which contains the device label and other metadata
 * @param bus_label pointer to a uint32_t variable that will be filled with the parent bus label if the function succeed
 *
 * @return STATUS_OK if the parent bus label is found
 */
Status merlin_external_device_get_parent_bus_label(struct platform_device_driver *drv, uint32_t *bus_label);

#endif /* MERLIN_HELPERS_H */
