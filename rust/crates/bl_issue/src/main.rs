//! 签发工具（仅在你方构建机使用）。生成 Ed25519 密钥对并签名 license JSON。

use anyhow::{Context, Result};
use bl_license::LicenseFile;
use clap::{Parser, Subcommand};
use ed25519_dalek::{Signer, SigningKey};
use rand::rngs::OsRng;
use std::path::PathBuf;

#[derive(Parser)]
#[command(name = "bl_issue", about = "BSPHP-VMP offline license issuer")]
struct Cli {
    #[command(subcommand)]
    cmd: Cmd,
}

#[derive(Subcommand)]
enum Cmd {
    /// 生成 signing.key（32 字节私钥）与 verifying.key（32 字节公钥）
    GenKey {
        #[arg(long, default_value = "signing.key")]
        secret_out: PathBuf,
        #[arg(long, default_value = "verifying.key")]
        public_out: PathBuf,
    },
    /// 签发 license.json（写入标准输出或 --out）
    Sign {
        #[arg(long)]
        secret: PathBuf,
        #[arg(long)]
        product: String,
        /// Unix 时间戳（秒）
        #[arg(long)]
        expiry_unix: u64,
        /// `*` 表示不绑机；否则填目标机的 fingerprint（`bl_demo fingerprint`）
        #[arg(long, default_value = "*")]
        hwid: String,
        #[arg(long)]
        out: Option<PathBuf>,
    },
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    match cli.cmd {
        Cmd::GenKey {
            secret_out,
            public_out,
        } => {
            let signing_key = SigningKey::generate(&mut OsRng);
            let verifying_key = signing_key.verifying_key();
            std::fs::write(&secret_out, signing_key.to_bytes())
                .with_context(|| format!("write {:?}", secret_out))?;
            std::fs::write(&public_out, verifying_key.to_bytes())
                .with_context(|| format!("write {:?}", public_out))?;
            eprintln!(
                "OK: wrote secret {:?} and public {:?}",
                secret_out, public_out
            );
        }
        Cmd::Sign {
            secret,
            product,
            expiry_unix,
            hwid,
            out,
        } => {
            let bytes = std::fs::read(&secret).context("read secret key")?;
            if bytes.len() != 32 {
                anyhow::bail!("secret must be 32 bytes (Ed25519 seed/key)");
            }
            let sk_arr: [u8; 32] = bytes.try_into().unwrap();
            let signing_key = SigningKey::from_bytes(&sk_arr);

            let mut lic = LicenseFile {
                product,
                expiry_unix,
                hwid,
                sig_hex: String::new(),
            };
            let msg = format!(
                "bl_lic/v1|{}|{}|{}",
                lic.product, lic.expiry_unix, lic.hwid
            );
            let sig = signing_key.sign(msg.as_bytes());
            lic.sig_hex = hex::encode(sig.to_bytes());

            let json = serde_json::to_string_pretty(&lic)?;
            match out {
                Some(p) => {
                    std::fs::write(&p, &json).with_context(|| format!("write {:?}", p))?;
                    eprintln!("OK: wrote {:?}", p);
                }
                None => println!("{}", json),
            }
        }
    }
    Ok(())
}
