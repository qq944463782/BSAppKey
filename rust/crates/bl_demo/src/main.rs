//! 示例宿主：校验 license 或打印本机 fingerprint。

use anyhow::{Context, Result};
use bl_license::{machine_fingerprint, verify_license_file};
use clap::{Parser, Subcommand};
use std::path::PathBuf;

#[derive(Parser)]
#[command(name = "bl_demo", about = "Sample app using bl_license")]
struct Cli {
    #[command(subcommand)]
    cmd: Cmd,
}

#[derive(Subcommand)]
enum Cmd {
    /// 打印本机 HWID（与 signing 时 --hwid 对齐）
    Fingerprint,
    /// 校验 license.json + verifying.key
    Verify {
        #[arg(long)]
        license: PathBuf,
        #[arg(long)]
        public_key: PathBuf,
        /// 时钟宽限（秒），例如 300
        #[arg(long, default_value_t = 0)]
        grace_secs: u64,
    },
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    match cli.cmd {
        Cmd::Fingerprint => {
            println!("{}", machine_fingerprint());
        }
        Cmd::Verify {
            license,
            public_key,
            grace_secs,
        } => {
            let claims = verify_license_file(&license, &public_key, grace_secs)
                .with_context(|| "verify_license_file")?;
            println!("OK");
            println!("  product: {}", claims.product);
            println!("  expiry_unix: {}", claims.expiry_unix);
            println!("  hwid rule: {}", claims.hwid);
        }
    }
    Ok(())
}
