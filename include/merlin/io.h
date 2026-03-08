// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

/**
 * @file io.h
 * @brief Generic I/O access functions
 *
 * This file declare generic I/O access functions, that are architecture specific.
 * The actual implementation of these functions is expected to be provided by the platform layer.
 *
 */

#ifndef MERLIN_IO_H
#define MERLIN_IO_H

#include <types.h>
#include <stdint.h>
#include <stddef.h>
#include <merlin/arch/asm-generic/io.h>
#include <merlin/helpers.h>

/** @brief Generic iowrite interface that implicitly handle multiple sizes */
#define merlin_iowrite(reg, T) _Generic((T),   \
              size_t:   merlin_iowrite32,      \
              uint32_t: merlin_iowrite32,      \
              uint16_t: merlin_iowrite16,      \
              uint8_t:  merlin_iowrite8        \
        ) (reg, T)

/**
 * @brief  Writes one byte at given address
 *
 * @param addr destination address
 * @param val byte to write
 *
 * @note this function is always inline
 */
/*@
  assigns *(uint8_t*)addr;
*/
__attribute__((always_inline))
static inline void merlin_iowrite8(size_t addr, uint8_t val)
{
#ifdef __FRAMAC__
    *(uint8_t*)addr = val;
#else
    __iowrite8(addr, val);
#endif
}

/**
 * @brief  Writes an half-word at given address
 *
 * @param addr destination address
 * @param val half-word to write
 *
 * @note this function is always inline
 */
/*@
  assigns *(uint16_t*)addr;
*/
__attribute__((always_inline))
static inline void  merlin_iowrite16(size_t addr, uint16_t val)
{
#ifdef __FRAMAC__
    *(uint16_t*)addr = val;
#else
    __iowrite16(addr, val);
#endif
}

/**
 * @brief  Writes a word at given address
 *
 * @param addr destination address
 * @param val word to write
 *
 * @note this function is always inline
 */
/*@
  assigns *(uint32_t*)addr;
*/
__attribute__((always_inline))
static inline void merlin_iowrite32(size_t addr, uint32_t val)
{
#ifdef __FRAMAC__
    *(uint32_t*)addr = val;
#else
    __iowrite32(addr, val);
#endif
}

/**
 * @brief  Reads one byte from given address
 *
 * @param addr source address
 * @return readden byte
 *
 * @note this function is always inline
 */
/*@
  assigns \nothing;
*/
__attribute__((always_inline))
static inline uint8_t merlin_ioread8(size_t addr)
{
#ifdef __FRAMAC__
    return *(uint8_t*)addr;
#else
    return __ioread8(addr);
#endif
}

/**
 * @brief  Reads an half-word from given address
 *
 * @param addr source address
 * @return readden half-word
 *
 * @note this function is always inline
 */
/*@
  assigns \nothing;
*/
__attribute__((always_inline))
static inline uint16_t merlin_ioread16(size_t addr)
{
#ifdef __FRAMAC__
    return *(uint16_t*)addr;
#else
    return __ioread16(addr);
#endif
}

/**
 * @brief  Reads a word from given address
 *
 * @param addr source address
 * @return readden word
 *
 * @note this function is always inline
 */
/*@
  assigns \nothing;
*/
__attribute__((always_inline))
static inline uint32_t merlin_ioread32(size_t addr)
{
#ifdef __FRAMAC__
    return *(uint32_t*)addr;
#else
    return __ioread32(addr);
#endif
}

/**
 * @brief poll register until all bits in bitmask are set
 *
 * @param addr register address
 * @param bitmask bitmask to wait for
 * @param nretry maximum number of try
 *
 * @return STATUS_OK if all bits are set, STATUS_BUSY if nretry is reached
 * without bitfield equality.
 */
static inline Status merlin_iopoll32_until_set(size_t addr, uint32_t bitmask, uint32_t nretry)
{
    Status status = STATUS_OK;
    uint32_t count = 0UL;
    uint32_t bitfield;

    do {
        bitfield = merlin_ioread32(addr) & bitmask;
        count++;
    } while ((bitfield != bitmask) && (count < nretry));

    if (unlikely(bitfield != bitmask)) {
        status = STATUS_BUSY;
    }
    return status;
}

/**
 * @brief poll register until all bits in bitmask are cleared
 *
 * @param addr register address
 * @param bitmask bitmask to wait for
 * @param nretry maximum number of try
 *
 * @return STATUS_OK if all bits are cleared, STATUS_BUSY if nretry is reached
 * without bitfield equality.
 */
static inline Status merlin_iopoll32_until_clear(size_t addr, uint32_t bitmask, uint32_t nretry)
{
    Status status = STATUS_OK;
    uint32_t count = 0UL;
    uint32_t bitfield;

    do {
        bitfield = merlin_ioread32(addr) & bitmask;
        count++;
    } while ((bitfield != 0) && (count < nretry));

    if (unlikely(bitfield != 0)) {
        status = STATUS_BUSY;
    }
    return status;
}

#endif /* MERLIN_IO_H */
