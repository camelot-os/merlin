//! DTS-backed metadata lookup helpers.

use crate::types::{DeviceType, UsbMaximumSpeed};

include!(concat!(env!("OUT_DIR"), "/dts_generated.rs"));

/// Returns the device metadata matching a Merlin device kind and DTS label.
///
/// The returned reference points to static metadata generated at build time
/// from the DTS backend.
///
/// Returns `None` when the label is unknown for the requested kind or when the
/// kind has no DTS-backed metadata (for example `DeviceType::Ext`).
pub fn lookup_devinfo(kind: DeviceType, label: u32) -> Option<&'static crate::types::DevInfo> {
    match kind {
        DeviceType::I2c => I2C_DEVICES
            .iter()
            .find(|dev| dev.devinfo.id == label)
            .map(|dev| &dev.devinfo),
        DeviceType::Spi => SPI_DEVICES
            .iter()
            .find(|dev| dev.devinfo.id == label)
            .map(|dev| &dev.devinfo),
        DeviceType::Usart => USART_DEVICES
            .iter()
            .find(|dev| dev.devinfo.id == label)
            .map(|dev| &dev.devinfo),
        DeviceType::Can => CAN_DEVICES
            .iter()
            .find(|dev| dev.devinfo.id == label)
            .map(|dev| &dev.devinfo),
        DeviceType::Usb => USB_DEVICES
            .iter()
            .find(|dev| dev.devinfo.id == label)
            .map(|dev| &dev.devinfo),
        DeviceType::Ext => None,
    }
}

/// Returns the input bus clock frequency in MHz for a labeled bus-backed device.
///
/// Supported kinds are I2C, SPI, USART and CAN. USB and EXT return `None`.
pub fn bus_clock_mhz(kind: DeviceType, label: u32) -> Option<u32> {
    match kind {
        DeviceType::I2c => I2C_DEVICES
            .iter()
            .find(|dev| dev.devinfo.id == label)
            .map(|dev| dev.bus_input_clock_freq),
        DeviceType::Spi => SPI_DEVICES
            .iter()
            .find(|dev| dev.devinfo.id == label)
            .map(|dev| dev.bus_input_clock_freq),
        DeviceType::Usart => USART_DEVICES
            .iter()
            .find(|dev| dev.devinfo.id == label)
            .map(|dev| dev.bus_input_clock_freq),
        DeviceType::Can => CAN_DEVICES
            .iter()
            .find(|dev| dev.devinfo.id == label)
            .map(|dev| dev.bus_input_clock_freq),
        DeviceType::Usb | DeviceType::Ext => None,
    }
}

/// Returns the maximum USB speed declared for the given USB device label.
pub fn usb_maximum_speed(label: u32) -> Option<UsbMaximumSpeed> {
    USB_DEVICES
        .iter()
        .find(|dev| dev.devinfo.id == label)
        .map(|dev| UsbMaximumSpeed::from(dev.maximum_speed))
}

/// Returns I2C child device metadata associated with an I2C controller label.
///
/// The slice length is clamped to the generated child array capacity to remain
/// robust even if DTS metadata and compile-time limits diverge.
pub fn i2c_child_devices(label: u32) -> Option<&'static [crate::types::DevInfo]> {
    I2C_DEVICES
        .iter()
        .find(|dev| dev.devinfo.id == label)
        .map(|dev| {
            let count = usize::try_from(dev.num_child_devices)
                .ok()
                .unwrap_or(0)
                .min(dev.child_devinfo.len());
            &dev.child_devinfo[..count]
        })
}
