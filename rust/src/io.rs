//! Generic MMIO register access helpers.
//!
//! This module mirrors Merlin C helpers from `include/merlin/io.h` for Rust
//! drivers and examples, while using a Rust-idiomatic generic API.
//!
//! # Safety model
//!
//! These helpers perform volatile reads and writes on raw addresses. Callers are
//! responsible for passing valid MMIO addresses with the expected width.

use core::ptr::{read_volatile, write_volatile};

use sentry_uapi::systypes::Status;

mod private {
    pub trait Sealed {}

    impl Sealed for u8 {}
    impl Sealed for u16 {}
    impl Sealed for u32 {}
}

/// Supported register value types for generic MMIO operations.
pub trait RegisterValue: private::Sealed + Copy {
    #[doc(hidden)]
    fn read_from(addr: usize) -> Self;
    #[doc(hidden)]
    fn write_to(addr: usize, val: Self);
}

impl RegisterValue for u8 {
    #[inline(always)]
    fn read_from(addr: usize) -> Self {
        // SAFETY: caller provides a valid MMIO address for an 8-bit read.
        unsafe { read_volatile(addr as *const u8) }
    }

    #[inline(always)]
    fn write_to(addr: usize, val: Self) {
        // SAFETY: caller provides a valid MMIO address for an 8-bit write.
        unsafe { write_volatile(addr as *mut u8, val) }
    }
}

impl RegisterValue for u16 {
    #[inline(always)]
    fn read_from(addr: usize) -> Self {
        // SAFETY: caller provides a valid MMIO address for a 16-bit read.
        unsafe { read_volatile(addr as *const u16) }
    }

    #[inline(always)]
    fn write_to(addr: usize, val: Self) {
        // SAFETY: caller provides a valid MMIO address for a 16-bit write.
        unsafe { write_volatile(addr as *mut u16, val) }
    }
}

impl RegisterValue for u32 {
    #[inline(always)]
    fn read_from(addr: usize) -> Self {
        // SAFETY: caller provides a valid MMIO address for a 32-bit read.
        unsafe { read_volatile(addr as *const u32) }
    }

    #[inline(always)]
    fn write_to(addr: usize, val: Self) {
        // SAFETY: caller provides a valid MMIO address for a 32-bit write.
        unsafe { write_volatile(addr as *mut u32, val) }
    }
}

/// Writes a typed value to a MMIO register address.
///
/// Register width is deduced from `T` (`u8`, `u16`, `u32`).
///
/// # Example
///
/// ```no_run
/// use camelot_merlin::io::iowrite;
///
/// const USART_CR1: usize = 0x4000_3800;
///
/// iowrite::<u32>(USART_CR1, 0x0000_0001);
/// ```
#[inline(always)]
pub fn iowrite<T: RegisterValue>(addr: usize, val: T) {
    T::write_to(addr, val)
}

/// Reads a typed value from a MMIO register address.
///
/// Register width is deduced from `T` (`u8`, `u16`, `u32`) via contextual type
/// inference or an explicit annotation.
///
/// # Example
///
/// ```no_run
/// use camelot_merlin::io::ioread;
///
/// const USART_SR: usize = 0x4000_3800;
///
/// let _status = ioread::<u32>(USART_SR);
/// ```
#[inline(always)]
pub fn ioread<T: RegisterValue>(addr: usize) -> T {
    T::read_from(addr)
}

/// Polls until all bits from `bitmask` are set in the 32-bit register.
///
/// Returns `Status::Ok` when all masked bits are set before `nretry` attempts,
/// otherwise returns `Status::Busy`.
///
/// # Example
///
/// ```no_run
/// use camelot_merlin::io::iopoll32_until_set;
///
/// const USART_ISR: usize = 0x4000_3800;
/// const TXE: u32 = 1 << 7;
///
/// let _status = iopoll32_until_set(USART_ISR, TXE, 128);
/// ```
#[inline(always)]
pub fn iopoll32_until_set(addr: usize, bitmask: u32, nretry: u32) -> Status {
    let mut count: u32 = 0;
    let mut bitfield: u32;

    loop {
        bitfield = ioread::<u32>(addr) & bitmask;
        count = count.saturating_add(1);
        if bitfield == bitmask || count >= nretry {
            break;
        }
    }

    if bitfield == bitmask {
        Status::Ok
    } else {
        Status::Busy
    }
}

/// Polls until all bits from `bitmask` are cleared in the 32-bit register.
///
/// Returns `Status::Ok` when all masked bits are cleared before `nretry`
/// attempts, otherwise returns `Status::Busy`.
///
/// # Example
///
/// ```no_run
/// use camelot_merlin::io::iopoll32_until_clear;
///
/// const USART_ISR: usize = 0x4000_3800;
/// const BUSY: u32 = 1 << 16;
///
/// let _status = iopoll32_until_clear(USART_ISR, BUSY, 128);
/// ```
#[inline(always)]
pub fn iopoll32_until_clear(addr: usize, bitmask: u32, nretry: u32) -> Status {
    let mut count: u32 = 0;
    let mut bitfield: u32;

    loop {
        bitfield = ioread::<u32>(addr) & bitmask;
        count = count.saturating_add(1);
        if bitfield == 0 || count >= nretry {
            break;
        }
    }

    if bitfield == 0 {
        Status::Ok
    } else {
        Status::Busy
    }
}
