# Rust examples

This directory contains `no_std` examples that use only:

- `camelot-merlin`
- `sentry-uapi`

The examples are source snippets to integrate in your own task crate.

## USART example

File: `stm32f4_usart_driver.rs`

Typical integration flow:

```rust
use camelot_merlin::platform::Merlin;
use sentry_uapi::systypes::Status;

// Copy or import the example module in your task crate.
use crate::stm32f4_usart_driver::usart_write_hello;

let mut merlin: Merlin<8> = Merlin::new();
let usart_label: u32 = 0x4000_3800; // replace with your DTS label

let status: Status = usart_write_hello(&mut merlin, usart_label);
if status != Status::Ok {
	// handle platform-specific error
}
```

Notes:

- The label value must match the generated DTS metadata used by `camelot-merlin`.
- The helper internally performs probe/init/write/flush/release.
