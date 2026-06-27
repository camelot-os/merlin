#![cfg_attr(not(feature = "std"), no_std)]

//! Rust implementation of Merlin runtime primitives.
//!
//! This crate provides no-std, `sentry-uapi`-based Merlin platform primitives,
//! including:
//!
//! - device registration from DTS labels
//! - map/unmap and GPIO/IRQ helpers
//! - IRQ dispatching to registered ISR callbacks
//! - build-time generated DTS metadata
//! - generic MMIO register helpers
//!
//! # Minimal usage
//!
//! ```no_run
//! use camelot_merlin::platform::Merlin;
//! use camelot_merlin::types::{DeviceType, PlatformDeviceDriver};
//!
//! let mut merlin: Merlin<8> = Merlin::new();
//! let mut driver = PlatformDeviceDriver::new("usart-demo", DeviceType::Usart);
//! let label: u32 = 0x4000_3800;
//!
//! let status = merlin.driver_register(&mut driver, label);
//! if status == camelot_merlin::Status::Ok {
//!     let _ = merlin.driver_map(&driver);
//!     let _ = merlin.driver_configure_gpio(&driver);
//!     let _ = merlin.driver_unmap(&driver);
//! }
//! ```

pub mod dts;
pub mod io;
pub mod platform;
pub mod types;

pub use sentry_uapi::systypes::Status;
