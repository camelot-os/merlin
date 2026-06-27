use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

const FALLBACK_DTS_RS: &str = r"// Auto-generated fallback (no DTS provided)
use crate::types::{CanDeviceInfo, I2cDeviceInfo, SpiDeviceInfo, UsbDeviceInfo, UsartDeviceInfo};

/// Generated I2C device metadata indexed by Merlin DTS label.
pub static I2C_DEVICES: &[I2cDeviceInfo] = &[];
/// Generated SPI device metadata indexed by Merlin DTS label.
pub static SPI_DEVICES: &[SpiDeviceInfo] = &[];
/// Generated USART device metadata indexed by Merlin DTS label.
pub static USART_DEVICES: &[UsartDeviceInfo] = &[];
/// Generated CAN device metadata indexed by Merlin DTS label.
pub static CAN_DEVICES: &[CanDeviceInfo] = &[];
/// Generated USB device metadata indexed by Merlin DTS label.
pub static USB_DEVICES: &[UsbDeviceInfo] = &[];
";

fn main() {
    println!("cargo:rerun-if-env-changed=MERLIN_DTS");
    println!("cargo:rerun-if-env-changed=MERLIN_CONFIG_TASK_LABEL");
    println!("cargo:rerun-if-env-changed=MERLIN_DTS2SRC");
    println!("cargo:rerun-if-env-changed=MERLIN_DTS_INCLUDE_DIRS");
    println!("cargo:rerun-if-env-changed=CPP");
    println!("cargo:rerun-if-changed=templates/dts_generated.rs.in");

    let out_dir = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR must be set by Cargo"));
    let generated = out_dir.join("dts_generated.rs");

    let dts = match env::var("MERLIN_DTS") {
        Ok(v) if !v.trim().is_empty() => PathBuf::from(v),
        _ => {
            fs::write(&generated, FALLBACK_DTS_RS)
                .expect("failed to write fallback dts_generated.rs");
            println!("cargo:warning=MERLIN_DTS is not set, generating empty DTS metadata backend");
            return;
        }
    };

    println!("cargo:rerun-if-changed={}", dts.display());

    let dts2src = env::var("MERLIN_DTS2SRC").unwrap_or_else(|_| "dts2src".to_string());
    let template = PathBuf::from("templates").join("dts_generated.rs.in");
    let task_label = env::var("MERLIN_CONFIG_TASK_LABEL").unwrap_or_else(|_| "0x0".to_string());

    let output = Command::new(&dts2src)
        .arg("-d")
        .arg(&dts)
        .arg("-t")
        .arg(&template)
        .arg(&generated)
        .env("CONFIG_TASK_LABEL", task_label)
        .output()
        .expect("failed to execute dts2src");

    if output.status.success() {
        return;
    }

    // Retry with C preprocessor for DTS files that rely on #include and macros.
    if let Some(preprocessed_dts) = preprocess_dts(&dts, &out_dir) {
        let retry = Command::new(&dts2src)
            .arg("-d")
            .arg(&preprocessed_dts)
            .arg("-t")
            .arg(&template)
            .arg(&generated)
            .env(
                "CONFIG_TASK_LABEL",
                env::var("MERLIN_CONFIG_TASK_LABEL").unwrap_or_else(|_| "0x0".to_string()),
            )
            .output()
            .expect("failed to execute dts2src on preprocessed DTS");

        if retry.status.success() {
            return;
        }

        panic_with_outputs(
            "dts2src failed (including preprocessed retry)",
            &output.stdout,
            &output.stderr,
            &retry.stdout,
            &retry.stderr,
        );
    }

    panic_with_outputs("dts2src failed", &output.stdout, &output.stderr, &[], &[]);
}

fn preprocess_dts(dts: &Path, out_dir: &Path) -> Option<PathBuf> {
    let cpp = env::var("CPP").unwrap_or_else(|_| "cpp".to_string());
    let include_paths = collect_include_dirs();
    if include_paths.is_empty() {
        return None;
    }

    let output = out_dir.join("preprocessed.dts");
    let mut cmd = Command::new(cpp);
    cmd.arg("-E")
        .arg("-P")
        .arg("-x")
        .arg("assembler-with-cpp")
        .arg("-undef")
        .arg("-nostdinc");
    for include in include_paths {
        cmd.arg("-I").arg(include);
    }
    let status = cmd.arg(dts).arg("-o").arg(&output).status().ok()?;
    if status.success() { Some(output) } else { None }
}

fn collect_include_dirs() -> Vec<PathBuf> {
    let mut include_paths = default_include_dirs();
    if let Ok(raw) = env::var("MERLIN_DTS_INCLUDE_DIRS") {
        include_paths.extend(split_include_dirs(&raw));
    }
    include_paths
}

fn default_include_dirs() -> Vec<PathBuf> {
    let manifest_dir = PathBuf::from(
        env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR must be set by Cargo"),
    );
    let root = manifest_dir.parent().unwrap_or(&manifest_dir);

    let candidates = [
        root.join("subprojects").join("devicetree").join("include"),
        root.join("subprojects")
            .join("devicetree")
            .join("dts")
            .join("arch"),
        root.join("subprojects")
            .join("devicetree")
            .join("dts")
            .join("manufacturer"),
        root.join("subprojects")
            .join("devicetree")
            .join("dts")
            .join("common"),
    ];

    candidates.into_iter().filter(|p| p.exists()).collect()
}

fn split_include_dirs(raw: &str) -> Vec<PathBuf> {
    let sep = if cfg!(windows) { ';' } else { ':' };
    raw.split(sep)
        .map(str::trim)
        .filter(|s| !s.is_empty())
        .map(PathBuf::from)
        .collect()
}

fn panic_with_outputs(
    msg: &str,
    stdout1: &[u8],
    stderr1: &[u8],
    stdout2: &[u8],
    stderr2: &[u8],
) -> ! {
    let mut full = String::from(msg);
    if !stdout1.is_empty() {
        full.push_str("\nfirst stdout:\n");
        full.push_str(&String::from_utf8_lossy(stdout1));
    }
    if !stderr1.is_empty() {
        full.push_str("\nfirst stderr:\n");
        full.push_str(&String::from_utf8_lossy(stderr1));
    }
    if !stdout2.is_empty() {
        full.push_str("\nretry stdout:\n");
        full.push_str(&String::from_utf8_lossy(stdout2));
    }
    if !stderr2.is_empty() {
        full.push_str("\nretry stderr:\n");
        full.push_str(&String::from_utf8_lossy(stderr2));
    }
    panic!("{full}");
}
