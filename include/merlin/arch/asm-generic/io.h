// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_ASM_GENERIC_IO_H
#define MERLIN_ASM_GENERIC_IO_H

#ifdef __arm__
#include <merlin/arch/asm-cortex-m/io.h>
#elif defined(__riscv_xlen) && __riscv_xlen == 32
#include <merlin/arch/asm-rv32/io.h>
#else
#error "unsupported architecture, no generic implementation of ioread/iowrite is provided for this"
#endif

#endif /* MERLIN_ASM_GENERIC_IO_H */
