use core::ptr::NonNull;

use sentry_uapi::systypes::DeviceHandle;

pub const MAX_IRQS: usize = 8;
pub const MAX_IOS: usize = 8;
pub const MAX_I2C_CHILDREN: usize = 4;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DeviceType {
    I2c = 1,
    Spi = 2,
    Usart = 3,
    Can = 4,
    Usb = 5,
    Ext = 6,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct ItInfo {
    pub it_controller: u32,
    pub it_num: u32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct DevIo {
    pub port: u32,
    pub pin: u32,
    pub mode: u32,
    pub af: u32,
    pub ppull: u32,
    pub speed: u32,
    pub pupdr: u32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct DevInfo {
    pub id: u32,
    pub baseaddr: usize,
    pub size: usize,
    pub num_interrupt: usize,
    pub its: [ItInfo; MAX_IRQS],
    pub num_ios: usize,
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
    pub fn contains_irq(self, irqn: u32) -> bool {
        self.its
            .iter()
            .take(self.num_interrupt.min(MAX_IRQS))
            .any(|irq| irq.it_num == irqn)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UsbMaximumSpeed {
    LowSpeed = 0,
    FullSpeed = 1,
    HighSpeed = 2,
    SuperSpeed = 3,
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

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct I2cDeviceInfo {
    pub devinfo: DevInfo,
    pub child_devinfo: [DevInfo; MAX_I2C_CHILDREN],
    pub num_child_devices: u32,
    pub bus_input_clock_freq: u32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct SpiDeviceInfo {
    pub devinfo: DevInfo,
    pub bus_input_clock_freq: u32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct UsartDeviceInfo {
    pub devinfo: DevInfo,
    pub bus_input_clock_freq: u32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct CanDeviceInfo {
    pub devinfo: DevInfo,
    pub bus_input_clock_freq: u32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct UsbDeviceInfo {
    pub devinfo: DevInfo,
    pub maximum_speed: u32,
}

pub type PlatformIsrFn = fn(&mut PlatformDeviceDriver, u32) -> i32;

#[derive(Debug, Clone, Copy, Default)]
pub struct PlatformFops {
    pub isr: Option<PlatformIsrFn>,
}

#[derive(Debug)]
pub struct PlatformDeviceDriver {
    pub devh: DeviceHandle,
    pub label: u32,
    pub devinfo: Option<&'static DevInfo>,
    pub name: &'static str,
    pub platform_fops: PlatformFops,
    pub kind: DeviceType,
    pub private_data: Option<NonNull<()>>,
}

impl PlatformDeviceDriver {
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
