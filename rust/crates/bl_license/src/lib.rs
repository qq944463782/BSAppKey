//! Offline license verification (Ed25519). Windows: HWID from `MachineGuid`.
//! BSPHP 联网激活可在后续版本用 `ureq` 等在此 crate 上扩展。

use ed25519_dalek::{Signature, Verifier, VerifyingKey};
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::path::Path;

const CANON_PREFIX: &str = "bl_lic/v1";

#[derive(Debug, thiserror::Error)]
pub enum LicenseError {
    #[error("IO: {0}")]
    Io(#[from] std::io::Error),
    #[error("JSON: {0}")]
    Json(#[from] serde_json::Error),
    #[error("invalid public key")]
    BadPublicKey,
    #[error("invalid signature encoding")]
    BadSignature,
    #[error("cryptographic verification failed")]
    BadCrypto(#[from] ed25519_dalek::SignatureError),
    #[error("license expired (now={now}, expiry={expiry})")]
    Expired { now: u64, expiry: u64 },
    #[error("machine binding mismatch (expected rule={rule}, actual={actual})")]
    HwidMismatch { rule: String, actual: String },
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LicenseFile {
    pub product: String,
    pub expiry_unix: u64,
    /// `"*"` = any machine; else must equal [`machine_fingerprint`].
    pub hwid: String,
    /// Hex-encoded 64-byte Ed25519 signature.
    pub sig_hex: String,
}

#[derive(Debug, Clone)]
pub struct LicenseClaims {
    pub product: String,
    pub expiry_unix: u64,
    pub hwid: String,
}

impl LicenseFile {
    fn canonical_message(&self) -> Vec<u8> {
        let s = format!(
            "{}|{}|{}|{}",
            CANON_PREFIX, self.product, self.expiry_unix, self.hwid
        );
        s.into_bytes()
    }

    pub fn verify(
        &self,
        verifying_key: &VerifyingKey,
        grace_skew_secs: u64,
    ) -> Result<LicenseClaims, LicenseError> {
        let msg = self.canonical_message();
        let sig_bytes = hex::decode(&self.sig_hex).map_err(|_| LicenseError::BadSignature)?;
        if sig_bytes.len() != 64 {
            return Err(LicenseError::BadSignature);
        }
        let sig_arr: [u8; 64] = sig_bytes
            .try_into()
            .map_err(|_| LicenseError::BadSignature)?;
        let sig = Signature::from_bytes(&sig_arr);
        verifying_key.verify(&msg, &sig)?;

        let now = unix_now();
        if now > self.expiry_unix.saturating_add(grace_skew_secs) {
            return Err(LicenseError::Expired {
                now,
                expiry: self.expiry_unix,
            });
        }

        let actual = machine_fingerprint();
        if self.hwid != "*" && self.hwid != actual {
            return Err(LicenseError::HwidMismatch {
                rule: self.hwid.clone(),
                actual,
            });
        }

        Ok(LicenseClaims {
            product: self.product.clone(),
            expiry_unix: self.expiry_unix,
            hwid: self.hwid.clone(),
        })
    }
}

/// Load Ed25519 verifying key from a 32-byte raw file.
pub fn load_verifying_key(path: &Path) -> Result<VerifyingKey, LicenseError> {
    let buf = std::fs::read(path)?;
    if buf.len() != 32 {
        return Err(LicenseError::BadPublicKey);
    }
    let arr: [u8; 32] = buf.try_into().map_err(|_| LicenseError::BadPublicKey)?;
    VerifyingKey::from_bytes(&arr).map_err(|_| LicenseError::BadPublicKey)
}

pub fn verify_license_file(
    license_path: &Path,
    public_key_path: &Path,
    grace_skew_secs: u64,
) -> Result<LicenseClaims, LicenseError> {
    let vk = load_verifying_key(public_key_path)?;
    let text = std::fs::read_to_string(license_path)?;
    let lic: LicenseFile = serde_json::from_str(&text)?;
    lic.verify(&vk, grace_skew_secs)
}

fn unix_now() -> u64 {
    use std::time::{SystemTime, UNIX_EPOCH};
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0)
}

/// SHA-256 hex (64 chars) of machine id (Windows: `MachineGuid`).
pub fn machine_fingerprint() -> String {
    let raw = machine_raw_id();
    let mut h = Sha256::new();
    h.update(raw.as_bytes());
    format!("{:x}", h.finalize())
}

fn machine_raw_id() -> String {
    #[cfg(windows)]
    {
        machine_guid_windows().unwrap_or_else(|| "no-machine-guid".into())
    }
    #[cfg(not(windows))]
    {
        std::env::var("BL_LICENSE_DEV_MACHINE").unwrap_or_else(|_| "non-windows-dev".into())
    }
}

#[cfg(windows)]
fn machine_guid_windows() -> Option<String> {
    use winreg::enums::*;
    use winreg::RegKey;
    let hklm = RegKey::predef(HKEY_LOCAL_MACHINE);
    let key = hklm.open_subkey("SOFTWARE\\Microsoft\\Cryptography").ok()?;
    let guid: String = key.get_value("MachineGuid").ok()?;
    Some(guid.to_lowercase())
}
