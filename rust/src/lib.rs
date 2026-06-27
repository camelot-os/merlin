#![cfg_attr(not(feature = "std"), no_std)]

//! Rust implementation of Merlin runtime primitives.
//!
//! This crate provides a no-std, `sentry-uapi`-based equivalent of Merlin C
//! platform primitives, including:
//!
//! - device registration from DTS labels
//! - map/unmap and GPIO/IRQ helpers
//! - IRQ dispatching to registered ISR callbacks
//! - build-time generated DTS metadata

pub mod dts;
pub mod platform;
pub mod types;

pub use sentry_uapi::systypes::Status;
