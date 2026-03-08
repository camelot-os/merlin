// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_HELPERS_H
#define MERLIN_HELPERS_H

#ifndef unlikely
/**
 * @def unlikely
 *
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

#endif /* MERLIN_HELPERS_H */
