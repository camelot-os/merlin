# camelot-merlin

Rust implementation of Merlin platform runtime, aligned with the C implementation model:

- registration by DTS label
- device handle retrieval through Sentry UAPI
- map/unmap helpers
- GPIO and IRQ helpers
- IRQ dispatch to registered ISR callbacks
- build-time DTS to Rust metadata generation

## Dependency policy

This crate depends only on `sentry-uapi = 0.4.8`.

## DTS generation

`build.rs` can generate Rust metadata from a DTS file through `dts2src`.

Environment variables:

- `MERLIN_DTS`: path to the DTS file
- `MERLIN_CONFIG_TASK_LABEL`: task label used for ownership filtering (hex string such as `0xC001F002`)
- `MERLIN_DTS2SRC`: optional path to the `dts2src` binary (default: `dts2src` in `PATH`)
- `MERLIN_DTS_INCLUDE_DIRS`: optional include roots (`:`-separated on Linux) appended to the default Merlin devicetree include roots used by `build.rs` when preprocessing DTS files with `#include`

If `MERLIN_DTS` is not set, an empty metadata backend is generated, which still allows compiling the crate.

## Cross compilation (thumbv8m.main-none-eabi)

Typical commands:

```bash
rustup target add thumbv8m.main-none-eabi
cargo check --target thumbv8m.main-none-eabi
cargo clippy --target thumbv8m.main-none-eabi -- -D warnings
```

With DTS generation (recommended: use a Meson-preprocessed DTS):

```bash
MERLIN_DTS=../builddir/subprojects/devicetree/sample.dts.pp \
MERLIN_CONFIG_TASK_LABEL=0xC001F002 \
cargo check --target thumbv8m.main-none-eabi
```

When using a raw DTS file, all label references (for example `&foo`) must be resolvable after preprocessing.
