//! Merlin runtime context and driver lifecycle helpers.
//!
//! The [`Merlin`] type stores the set of registered drivers, resolves static
//! DTS metadata, and centralizes GPIO/IRQ related operations.

use sentry_uapi::systypes::{DeviceHandle, Status};
use sentry_uapi::{copy_from_kernel, syscall};

use crate::dts;
use crate::types::{DeviceType, PlatformDeviceDriver};

#[derive(Clone, Copy)]
struct RegisteredDriver {
    ptr: *mut PlatformDeviceDriver,
}

/// Merlin runtime context.
///
/// The C implementation keeps a global list of registered drivers. This Rust
/// implementation keeps that state in an explicit context object.
///
/// # Example
///
/// ```no_run
/// use camelot_merlin::platform::Merlin;
///
/// let _merlin: Merlin<4> = Merlin::new();
/// ```
pub struct Merlin<const MAX_REGISTERED_DRIVERS: usize> {
    registered_drivers: [Option<RegisteredDriver>; MAX_REGISTERED_DRIVERS],
    num_registered_drivers: usize,
}

impl<const MAX_REGISTERED_DRIVERS: usize> Default for Merlin<MAX_REGISTERED_DRIVERS> {
    fn default() -> Self {
        Self::new()
    }
}

impl<const MAX_REGISTERED_DRIVERS: usize> Merlin<MAX_REGISTERED_DRIVERS> {
    /// Creates an empty Merlin runtime context.
    ///
    /// `MAX_REGISTERED_DRIVERS` defines the maximum number of drivers that can
    /// be registered in this context.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use camelot_merlin::platform::Merlin;
    ///
    /// let _merlin: Merlin<8> = Merlin::new();
    /// ```
    pub const fn new() -> Self {
        Self {
            registered_drivers: [None; MAX_REGISTERED_DRIVERS],
            num_registered_drivers: 0,
        }
    }

    /// Registers a platform driver from its DTS label.
    ///
    /// On success, the driver is populated with the kernel device handle and
    /// static device metadata.
    ///
    /// Returns:
    /// - `Status::Ok` on success
    /// - `Status::Busy` when the runtime registration table is full
    /// - `Status::Invalid` when label lookup or handle retrieval fails
    ///
    /// # Example
    ///
    /// ```no_run
    /// use camelot_merlin::platform::Merlin;
    /// use camelot_merlin::types::{DeviceType, PlatformDeviceDriver};
    ///
    /// let mut merlin: Merlin<4> = Merlin::new();
    /// let mut driver = PlatformDeviceDriver::new("usart-demo", DeviceType::Usart);
    /// let label = 0x4000_3800;
    ///
    /// let _status = merlin.driver_register(&mut driver, label);
    /// ```
    pub fn driver_register(&mut self, driver: &mut PlatformDeviceDriver, label: u32) -> Status {
        if self.num_registered_drivers >= MAX_REGISTERED_DRIVERS {
            return Status::Busy;
        }

        let Some(devinfo) = dts::lookup_devinfo(driver.kind, label) else {
            return Status::Invalid;
        };

        let Ok(devh) = kernel_get_device_handle(label) else {
            return Status::Invalid;
        };

        driver.devh = devh;
        driver.label = label;
        driver.devinfo = Some(devinfo);

        self.registered_drivers[self.num_registered_drivers] = Some(RegisteredDriver {
            ptr: driver as *mut PlatformDeviceDriver,
        });
        self.num_registered_drivers += 1;

        Status::Ok
    }

    /// Maps the device memory region for a registered driver.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use camelot_merlin::platform::Merlin;
    /// use camelot_merlin::types::{DeviceType, PlatformDeviceDriver};
    ///
    /// let merlin: Merlin<4> = Merlin::new();
    /// let driver = PlatformDeviceDriver::new("usart-demo", DeviceType::Usart);
    ///
    /// let _status = merlin.driver_map(&driver);
    /// ```
    pub fn driver_map(&self, driver: &PlatformDeviceDriver) -> Status {
        syscall::map_dev(driver.devh)
    }

    /// Unmaps the device memory region previously mapped for a driver.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use camelot_merlin::platform::Merlin;
    /// use camelot_merlin::types::{DeviceType, PlatformDeviceDriver};
    ///
    /// let merlin: Merlin<4> = Merlin::new();
    /// let driver = PlatformDeviceDriver::new("usart-demo", DeviceType::Usart);
    ///
    /// let _status = merlin.driver_unmap(&driver);
    /// ```
    pub fn driver_unmap(&self, driver: &PlatformDeviceDriver) -> Status {
        syscall::unmap_dev(driver.devh)
    }

    /// Configures GPIOs declared in the driver's DTS metadata.
    ///
    /// Returns `Status::NoEntity` if no metadata is attached to the driver.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use camelot_merlin::platform::Merlin;
    /// use camelot_merlin::types::{DeviceType, PlatformDeviceDriver};
    ///
    /// let merlin: Merlin<4> = Merlin::new();
    /// let driver = PlatformDeviceDriver::new("usart-demo", DeviceType::Usart);
    ///
    /// let _status = merlin.driver_configure_gpio(&driver);
    /// ```
    pub fn driver_configure_gpio(&self, driver: &PlatformDeviceDriver) -> Status {
        let Some(devinfo) = driver.devinfo else {
            return Status::NoEntity;
        };

        for io in 0..devinfo.num_ios.min(crate::types::MAX_IOS) {
            let io_u8 = match u8::try_from(io) {
                Ok(v) => v,
                Err(_) => return Status::Invalid,
            };

            let status = syscall::gpio_configure(driver.devh, io_u8);
            if status != Status::Ok {
                return status;
            }
        }

        Status::Ok
    }

    /// Enables all interrupts declared in the driver's DTS metadata.
    ///
    /// Returns `Status::NoEntity` when no IRQ is available for that device.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use camelot_merlin::platform::Merlin;
    /// use camelot_merlin::types::{DeviceType, PlatformDeviceDriver};
    ///
    /// let merlin: Merlin<4> = Merlin::new();
    /// let driver = PlatformDeviceDriver::new("usart-demo", DeviceType::Usart);
    ///
    /// let _status = merlin.driver_enable_irqs(&driver);
    /// ```
    pub fn driver_enable_irqs(&self, driver: &PlatformDeviceDriver) -> Status {
        let Some(devinfo) = driver.devinfo else {
            return Status::NoEntity;
        };

        if devinfo.num_interrupt == 0 {
            return Status::NoEntity;
        }

        for irq in devinfo.its.iter().take(devinfo.num_interrupt) {
            let Ok(irqn) = u16::try_from(irq.it_num) else {
                return Status::Invalid;
            };
            let status = syscall::irq_enable(irqn);
            if status != Status::Ok {
                return status;
            }
        }

        Status::Ok
    }

    /// Disables all interrupts declared in the driver's DTS metadata.
    ///
    /// Returns `Status::NoEntity` when no IRQ is available for that device.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use camelot_merlin::platform::Merlin;
    /// use camelot_merlin::types::{DeviceType, PlatformDeviceDriver};
    ///
    /// let merlin: Merlin<4> = Merlin::new();
    /// let driver = PlatformDeviceDriver::new("usart-demo", DeviceType::Usart);
    ///
    /// let _status = merlin.driver_disable_irqs(&driver);
    /// ```
    pub fn driver_disable_irqs(&self, driver: &PlatformDeviceDriver) -> Status {
        let Some(devinfo) = driver.devinfo else {
            return Status::NoEntity;
        };

        if devinfo.num_interrupt == 0 {
            return Status::NoEntity;
        }

        for irq in devinfo.its.iter().take(devinfo.num_interrupt) {
            let Ok(irqn) = u16::try_from(irq.it_num) else {
                return Status::Invalid;
            };
            let status = syscall::irq_disable(irqn);
            if status != Status::Ok {
                return status;
            }
        }

        Status::Ok
    }

    /// Returns bus clock frequency in MHz for bus-backed drivers.
    ///
    /// Valid for I2C/SPI/USART/CAN. Returns `Status::Invalid` for unsupported
    /// kinds or unknown labels.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use camelot_merlin::platform::Merlin;
    /// use camelot_merlin::types::{DeviceType, PlatformDeviceDriver};
    ///
    /// let merlin: Merlin<4> = Merlin::new();
    /// let driver = PlatformDeviceDriver::new("spi-demo", DeviceType::Spi);
    ///
    /// let _clock_mhz = merlin.driver_get_bus_clock(&driver);
    /// ```
    pub fn driver_get_bus_clock(&self, driver: &PlatformDeviceDriver) -> Result<u32, Status> {
        match driver.kind {
            DeviceType::I2c | DeviceType::Spi | DeviceType::Usart | DeviceType::Can => {
                dts::bus_clock_mhz(driver.kind, driver.label).ok_or(Status::Invalid)
            }
            DeviceType::Usb | DeviceType::Ext => Err(Status::Invalid),
        }
    }

    /// Dispatches a raised IRQ to the matching registered driver ISR.
    ///
    /// If an ISR callback is installed, it is called before IRQ acknowledge and
    /// re-enable operations.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use camelot_merlin::platform::Merlin;
    ///
    /// let mut merlin: Merlin<4> = Merlin::new();
    /// let _status = merlin.irq_dispatch(42);
    /// ```
    pub fn irq_dispatch(&mut self, irqn: u32) -> Status {
        let Some(driver) = self.find_driver_from_irq(irqn) else {
            return Status::Invalid;
        };

        if let Some(isr) = driver.platform_fops.isr {
            let _ = isr(driver, irqn);
        }

        let Some(devinfo) = driver.devinfo else {
            return Status::NoEntity;
        };

        for irq in devinfo.its.iter().take(devinfo.num_interrupt) {
            if irq.it_num == irqn {
                let Ok(raw_irqn) = u16::try_from(irqn) else {
                    return Status::Invalid;
                };

                let ack = syscall::irq_acknowledge(raw_irqn);
                if ack != Status::Ok {
                    return ack;
                }
                return syscall::irq_enable(raw_irqn);
            }
        }

        Status::NoEntity
    }

    fn find_driver_from_irq(&mut self, irqn: u32) -> Option<&mut PlatformDeviceDriver> {
        for slot in self
            .registered_drivers
            .iter_mut()
            .take(self.num_registered_drivers)
            .flatten()
        {
            // SAFETY: pointers are inserted from unique &mut references provided
            // by `driver_register`; this context models the monothreaded Merlin usage.
            let driver = unsafe { &mut *slot.ptr };
            if let Some(devinfo) = driver.devinfo {
                if devinfo.contains_irq(irqn) {
                    return Some(driver);
                }
            }
        }
        None
    }
}

fn kernel_get_device_handle(label: u32) -> Result<DeviceHandle, Status> {
    let status = syscall::get_device_handle(label);
    if status != Status::Ok {
        return Err(status);
    }

    let mut devh: DeviceHandle = 0;
    match copy_from_kernel(&mut devh) {
        Ok(Status::Ok) => Ok(devh),
        Ok(other) => Err(other),
        Err(err) => Err(err),
    }
}
