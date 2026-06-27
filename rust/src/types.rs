//! Public data types shared by Merlin runtime helpers.
//!
//! These types describe generated DTS metadata, driver descriptors and callback
//! conventions used by the [`crate::dts`] and [`crate::platform`] modules.

use core::ptr::NonNull;

use sentry_uapi::systypes::DeviceHandle;

/// Maximum number of interrupts stored in generated static metadata.
pub const MAX_IRQS: usize = 8;
/// Maximum number of GPIO/IO descriptors stored in generated metadata.
pub const MAX_IOS: usize = 8;
/// Maximum number of child devices attached to a generated I2C controller.
pub const MAX_I2C_CHILDREN: usize = 4;

/// Merlin-supported platform device families.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DeviceType {
    /// I2C controller or bus-backed I2C device.
    I2c = 1,
    /// SPI controller.
    Spi = 2,
    /// USART or UART-like controller.
    Usart = 3,
    /// CAN controller.
    Can = 4,
    /// USB controller.
    Usb = 5,
    /// External or unsupported device kind without DTS metadata backend.
    Ext = 6,
}

/// Interrupt metadata associated with a device.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct ItInfo {
    /// Interrupt controller identifier as declared by the DTS backend.
    pub it_controller: u32,
    /// Interrupt line number handled by the device.
    pub it_num: u32,
}

/// GPIO/IO metadata associated with a device.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct DevIo {
    /// GPIO port identifier.
    pub port: u32,
    /// GPIO pin number within the port.
    pub pin: u32,
    /// SoC-specific pin mode value.
    pub mode: u32,
    /// Alternate-function selector.
    pub af: u32,
    /// Push/pull configuration value.
    pub ppull: u32,
    /// SoC-specific slew-rate or speed configuration value.
    pub speed: u32,
    /// Pull-up or pull-down configuration value.
    pub pupdr: u32,
}

/// Core device metadata generated from DTS.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct DevInfo {
    /// Merlin DTS label used to retrieve the kernel device handle.
    pub id: u32,
    /// MMIO base address of the device register space.
    pub baseaddr: usize,
    /// Size, in bytes, of the MMIO region.
    pub size: usize,
    /// Number of valid entries stored in [`Self::its`].
    pub num_interrupt: usize,
    /// Interrupt descriptors associated with the device.
    pub its: [ItInfo; MAX_IRQS],
    /// Number of valid entries stored in [`Self::ios`].
    pub num_ios: usize,
    /// GPIO descriptors associated with the device.
    pub ios: [DevIo; MAX_IOS],
}

impl Default for DevInfo {
    fn default() -> Self {
        Self {
            id: 0,
            baseaddr: 0,
            size: 0,
            num_interrupt: 0,
            its: [ItInfo::default(); MAX_IRQS],
            num_ios: 0,
            ios: [DevIo::default(); MAX_IOS],
        }
    }
}

impl DevInfo {
    /// Returns `true` when `irqn` is present in this device interrupt table.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use camelot_merlin::types::DevInfo;
    ///
    /// let devinfo = DevInfo::default();
    /// assert!(!devinfo.contains_irq(5));
    /// ```
    pub fn contains_irq(self, irqn: u32) -> bool {
        self.its
            .iter()
            .take(self.num_interrupt.min(MAX_IRQS))
            .any(|irq| irq.it_num == irqn)
    }
}

/// USB speed capability as declared in DTS metadata.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UsbMaximumSpeed {
    /// USB low-speed mode.
    LowSpeed = 0,
    /// USB full-speed mode.
    FullSpeed = 1,
    /// USB high-speed mode.
    HighSpeed = 2,
    /// USB super-speed mode.
    SuperSpeed = 3,
    /// Speed is missing or not recognized by the generated backend.
    Unknown = 4,
}

impl From<u32> for UsbMaximumSpeed {
    fn from(value: u32) -> Self {
        match value {
            0 => Self::LowSpeed,
            1 => Self::FullSpeed,
            2 => Self::HighSpeed,
            3 => Self::SuperSpeed,
            _ => Self::Unknown,
        }
    }
}

/// Generated metadata record for an I2C controller and its children.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct I2cDeviceInfo {
    /// Core metadata for the controller itself.
    pub devinfo: DevInfo,
    /// Child devices discovered below the controller node.
    pub child_devinfo: [DevInfo; MAX_I2C_CHILDREN],
    /// Number of valid child entries stored in [`Self::child_devinfo`].
    pub num_child_devices: u32,
    /// Input peripheral clock expressed in MHz.
    pub bus_input_clock_freq: u32,
}

/// Generated metadata record for a SPI controller.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct SpiDeviceInfo {
    /// Core metadata for the controller.
    pub devinfo: DevInfo,
    /// Input peripheral clock expressed in MHz.
    pub bus_input_clock_freq: u32,
}

/// Generated metadata record for a USART controller.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct UsartDeviceInfo {
    /// Core metadata for the controller.
    pub devinfo: DevInfo,
    /// Input peripheral clock expressed in MHz.
    pub bus_input_clock_freq: u32,
}

/// Generated metadata record for a CAN controller.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct CanDeviceInfo {
    /// Core metadata for the controller.
    pub devinfo: DevInfo,
    /// Input peripheral clock expressed in MHz.
    pub bus_input_clock_freq: u32,
}

/// Generated metadata record for a USB controller.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct UsbDeviceInfo {
    /// Core metadata for the controller.
    pub devinfo: DevInfo,
    /// Encoded speed value converted through [`UsbMaximumSpeed::from`].
    pub maximum_speed: u32,
}

/// Interrupt service routine callback prototype for platform drivers.
pub type PlatformIsrFn = fn(&mut PlatformDeviceDriver, u32) -> i32;

/// Optional callbacks attached to a platform driver.
#[derive(Debug, Clone, Copy, Default)]
pub struct PlatformFops {
    /// Optional ISR callback invoked by [`crate::platform::Merlin::irq_dispatch`].
    pub isr: Option<PlatformIsrFn>,
}

/// Runtime state for one registered platform driver.
#[derive(Debug)]
pub struct PlatformDeviceDriver {
    /// Kernel device handle retrieved during registration.
    pub devh: DeviceHandle,
    /// Merlin DTS label used for registration.
    pub label: u32,
    /// Attached generated metadata, if registration succeeded.
    pub devinfo: Option<&'static DevInfo>,
    /// Human-readable driver name.
    pub name: &'static str,
    /// Optional function table used during IRQ dispatch.
    pub platform_fops: PlatformFops,
    /// Merlin device family handled by this driver.
    pub kind: DeviceType,
    /// Opaque driver-owned pointer for runtime private data.
    pub private_data: Option<NonNull<()>>,
}

impl PlatformDeviceDriver {
    /// Creates a new platform driver descriptor.
    ///
    /// Runtime fields (`devh`, `label`, `devinfo`) are filled by
    /// [`crate::platform::Merlin::driver_register`].
    ///
    /// # Example
    ///
    /// ```no_run
    /// use camelot_merlin::types::{DeviceType, PlatformDeviceDriver};
    ///
    /// let driver = PlatformDeviceDriver::new("spi-demo", DeviceType::Spi);
    /// assert_eq!(driver.name, "spi-demo");
    /// ```
    pub const fn new(name: &'static str, kind: DeviceType) -> Self {
        Self {
            devh: 0,
            label: 0,
            devinfo: None,
            name,
            platform_fops: PlatformFops { isr: None },
            kind,
            private_data: None,
        }
    }
}
