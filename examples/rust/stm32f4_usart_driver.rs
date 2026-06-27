#![no_std]

use core::ptr::{read_volatile, write_volatile};

use camelot_merlin::platform::Merlin;
use camelot_merlin::types::{DeviceType, PlatformDeviceDriver};
use sentry_uapi::systypes::Status;

const USART_CR1_OFFSET: usize = 0x00;
const USART_CR2_OFFSET: usize = 0x04;
const USART_CR3_OFFSET: usize = 0x08;
const USART_BRR_OFFSET: usize = 0x0c;
const USART_ISR_OFFSET: usize = 0x1c;
const USART_ICR_OFFSET: usize = 0x20;
const USART_RDR_OFFSET: usize = 0x24;
const USART_TDR_OFFSET: usize = 0x28;

const USART_CR1_UE: u32 = 1 << 0;
const USART_CR1_RE: u32 = 1 << 2;
const USART_CR1_TE: u32 = 1 << 3;
const USART_CR1_PCE: u32 = 1 << 10;
const USART_CR1_PS: u32 = 1 << 9;
const USART_CR1_M0: u32 = 1 << 12;
const USART_CR1_M1: u32 = 1 << 28;

const USART_CR2_STOP_SHIFT: u32 = 12;
const USART_CR2_STOP_MASK: u32 = 0x3 << USART_CR2_STOP_SHIFT;
const USART_CR2_STOP_1: u32 = 0x0 << USART_CR2_STOP_SHIFT;
const USART_CR2_STOP_2: u32 = 0x2 << USART_CR2_STOP_SHIFT;

const USART_ISR_RXNE: u32 = 1 << 5;
const USART_ISR_TC: u32 = 1 << 6;
const USART_ISR_TXE: u32 = 1 << 7;
const USART_ISR_ORE: u32 = 1 << 3;
const USART_ISR_NE: u32 = 1 << 2;
const USART_ISR_FE: u32 = 1 << 1;
const USART_ISR_PE: u32 = 1 << 0;

const USART_ICR_ORECF: u32 = 1 << 3;
const USART_ICR_NECF: u32 = 1 << 2;
const USART_ICR_FECF: u32 = 1 << 1;
const USART_ICR_PECF: u32 = 1 << 0;
const USART_ICR_TCCF: u32 = 1 << 6;

const USART_ERROR_MASK: u32 = USART_ISR_ORE | USART_ISR_NE | USART_ISR_FE | USART_ISR_PE;
const USART_ERROR_CLEAR: u32 = USART_ICR_ORECF | USART_ICR_NECF | USART_ICR_FECF | USART_ICR_PECF;
const USART_POLL_RETRIES: usize = 10_000;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UsartWordLength {
    Bits7,
    Bits8,
    Bits9,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UsartParity {
    None,
    Even,
    Odd,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UsartStopBits {
    One,
    Two,
}

#[derive(Clone, Copy, Debug)]
pub struct UsartConfig {
    pub baudrate: u32,
    pub word_length: UsartWordLength,
    pub parity: UsartParity,
    pub stop_bits: UsartStopBits,
    pub enable_tx: bool,
    pub enable_rx: bool,
}

impl Default for UsartConfig {
    fn default() -> Self {
        Self {
            baudrate: 115_200,
            word_length: UsartWordLength::Bits8,
            parity: UsartParity::None,
            stop_bits: UsartStopBits::One,
            enable_tx: true,
            enable_rx: true,
        }
    }
}

pub struct Stm32f4UsartDriver {
    platform: PlatformDeviceDriver,
    baseaddr: usize,
    initialized: bool,
}

impl Stm32f4UsartDriver {
    pub const fn new() -> Self {
        Self {
            platform: PlatformDeviceDriver::new("stm32f4-usart", DeviceType::Usart),
            baseaddr: 0,
            initialized: false,
        }
    }

    pub fn probe<const MAX_DRIVERS: usize>(
        &mut self,
        merlin: &mut Merlin<MAX_DRIVERS>,
        label: u32,
    ) -> Status {
        merlin.driver_register(&mut self.platform, label)
    }

    pub fn init<const MAX_DRIVERS: usize>(
        &mut self,
        merlin: &Merlin<MAX_DRIVERS>,
        config: UsartConfig,
    ) -> Status {
        let map_status = merlin.driver_map(&self.platform);
        if map_status != Status::Ok {
            return map_status;
        }

        let gpio_status = merlin.driver_configure_gpio(&self.platform);
        if gpio_status != Status::Ok {
            return gpio_status;
        }

        let Some(devinfo) = self.platform.devinfo else {
            return Status::NoEntity;
        };
        if devinfo.baseaddr == 0 {
            return Status::Invalid;
        }

        self.baseaddr = devinfo.baseaddr;
        let cfg_status = self.configure_hw(merlin, config);
        if cfg_status != Status::Ok {
            return cfg_status;
        }

        self.initialized = true;
        Status::Ok
    }

    pub fn release<const MAX_DRIVERS: usize>(&mut self, merlin: &Merlin<MAX_DRIVERS>) -> Status {
        if !self.initialized {
            return Status::NoEntity;
        }

        self.disable();
        self.initialized = false;
        self.baseaddr = 0;

        merlin.driver_unmap(&self.platform)
    }

    pub fn write_byte(&self, data: u8) -> Status {
        if !self.initialized {
            return Status::NoEntity;
        }
        if self.poll_until_set(USART_ISR_OFFSET, USART_ISR_TXE) != Status::Ok {
            return Status::Busy;
        }

        self.write_reg(USART_TDR_OFFSET, u32::from(data));
        Status::Ok
    }

    pub fn read_byte(&self) -> Result<u8, Status> {
        if !self.initialized {
            return Err(Status::NoEntity);
        }
        if self.poll_until_set(USART_ISR_OFFSET, USART_ISR_RXNE) != Status::Ok {
            return Err(Status::Busy);
        }

        if self.clear_rx_errors() != Status::Ok {
            return Err(Status::Busy);
        }

        Ok((self.read_reg(USART_RDR_OFFSET) & 0xff) as u8)
    }

    pub fn write_blocking(&self, bytes: &[u8]) -> Status {
        if bytes.is_empty() {
            return Status::Invalid;
        }

        for &b in bytes {
            let status = self.write_byte(b);
            if status != Status::Ok {
                return status;
            }
        }
        Status::Ok
    }

    pub fn flush(&self) -> Status {
        if !self.initialized {
            return Status::NoEntity;
        }

        if self.poll_until_set(USART_ISR_OFFSET, USART_ISR_TC) != Status::Ok {
            return Status::Busy;
        }
        self.write_reg(USART_ICR_OFFSET, USART_ICR_TCCF);
        Status::Ok
    }

    fn configure_hw<const MAX_DRIVERS: usize>(
        &mut self,
        merlin: &Merlin<MAX_DRIVERS>,
        config: UsartConfig,
    ) -> Status {
        if config.baudrate == 0 {
            return Status::Invalid;
        }

        let Ok(bus_clock_mhz) = merlin.driver_get_bus_clock(&self.platform) else {
            return Status::Invalid;
        };

        self.disable();

        let bus_hz = u64::from(bus_clock_mhz) * 1_000_000;
        let brr = bus_hz / u64::from(config.baudrate);
        if brr == 0 || brr > u64::from(u32::MAX) {
            return Status::Invalid;
        }

        self.write_reg(USART_BRR_OFFSET, brr as u32);

        let mut cr1 = self.read_reg(USART_CR1_OFFSET);
        cr1 &= !(USART_CR1_M0 | USART_CR1_M1 | USART_CR1_PCE | USART_CR1_PS | USART_CR1_TE | USART_CR1_RE);

        match config.word_length {
            UsartWordLength::Bits7 => cr1 |= USART_CR1_M1,
            UsartWordLength::Bits8 => {}
            UsartWordLength::Bits9 => cr1 |= USART_CR1_M0,
        }

        match config.parity {
            UsartParity::None => {}
            UsartParity::Even => cr1 |= USART_CR1_PCE,
            UsartParity::Odd => cr1 |= USART_CR1_PCE | USART_CR1_PS,
        }

        if config.enable_tx {
            cr1 |= USART_CR1_TE;
        }
        if config.enable_rx {
            cr1 |= USART_CR1_RE;
        }

        self.write_reg(USART_CR1_OFFSET, cr1);

        let mut cr2 = self.read_reg(USART_CR2_OFFSET);
        cr2 &= !USART_CR2_STOP_MASK;
        cr2 |= match config.stop_bits {
            UsartStopBits::One => USART_CR2_STOP_1,
            UsartStopBits::Two => USART_CR2_STOP_2,
        };
        self.write_reg(USART_CR2_OFFSET, cr2);

        self.write_reg(USART_CR3_OFFSET, self.read_reg(USART_CR3_OFFSET));
        self.enable();

        Status::Ok
    }

    fn clear_rx_errors(&self) -> Status {
        let isr = self.read_reg(USART_ISR_OFFSET);
        if (isr & USART_ERROR_MASK) != 0 {
            self.write_reg(USART_ICR_OFFSET, USART_ERROR_CLEAR);
            return Status::Busy;
        }
        Status::Ok
    }

    fn poll_until_set(&self, offset: usize, mask: u32) -> Status {
        for _ in 0..USART_POLL_RETRIES {
            if (self.read_reg(offset) & mask) != 0 {
                return Status::Ok;
            }
        }
        Status::Busy
    }

    fn enable(&self) {
        let cr1 = self.read_reg(USART_CR1_OFFSET) | USART_CR1_UE;
        self.write_reg(USART_CR1_OFFSET, cr1);
    }

    fn disable(&self) {
        let cr1 = self.read_reg(USART_CR1_OFFSET) & !USART_CR1_UE;
        self.write_reg(USART_CR1_OFFSET, cr1);
    }

    fn read_reg(&self, offset: usize) -> u32 {
        // SAFETY: base address is configured from DTS metadata after successful map.
        unsafe { read_volatile(self.reg_ptr(offset) as *const u32) }
    }

    fn write_reg(&self, offset: usize, value: u32) {
        // SAFETY: base address is configured from DTS metadata after successful map.
        unsafe { write_volatile(self.reg_ptr(offset), value) }
    }

    fn reg_ptr(&self, offset: usize) -> *mut u32 {
        (self.baseaddr + offset) as *mut u32
    }
}

pub fn usart_write_hello<const MAX_DRIVERS: usize>(
    merlin: &mut Merlin<MAX_DRIVERS>,
    label: u32,
) -> Status {
    let mut driver = Stm32f4UsartDriver::new();

    let status = driver.probe(merlin, label);
    if status != Status::Ok {
        return status;
    }

    let status = driver.init(merlin, UsartConfig::default());
    if status != Status::Ok {
        return status;
    }

    let status = driver.write_blocking(b"Hello from Rust USART over Merlin\r\n");
    if status != Status::Ok {
        return status;
    }

    let status = driver.flush();
    if status != Status::Ok {
        return status;
    }

    driver.release(merlin)
}
