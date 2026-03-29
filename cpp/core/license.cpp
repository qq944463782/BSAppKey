#include "license.h"
#include "base64.h"
#include "rsa_cng.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

static std::string trim(std::string s) {
  auto notsp = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), notsp));
  s.erase(std::find_if(s.rbegin(), s.rend(), notsp).base(), s.end());
  return s;
}

std::string LicenseCanonicalUtf8(const std::string& product, uint64_t expiry_unix,
                                 const std::string& hwid) {
  return std::string("BLCPP1|") + product + "|" + std::to_string(expiry_unix) + "|" + hwid;
}

std::string LicensePackUtf8(const std::string& product, uint64_t expiry_unix,
                            const std::string& hwid, const std::string& sig_b64_ascii) {
  std::ostringstream o;
  o << "BLCPP1\n";
  o << "product=" << product << "\n";
  o << "expiry=" << expiry_unix << "\n";
  o << "hwid=" << hwid << "\n";
  o << "sig_b64=" << sig_b64_ascii << "\n";
  return o.str();
}

bool LicenseParseInner(const std::string& inner_utf8, LicenseFields& out, std::string& err) {
  err.clear();
  out = {};
  std::istringstream in(inner_utf8);
  std::string line;
  bool head = false;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    line = trim(line);
    if (line.empty()) continue;
    if (line == "BLCPP1") {
      head = true;
      continue;
    }
    auto eq = line.find('=');
    if (eq == std::string::npos) {
      err = "bad line";
      return false;
    }
    std::string k = trim(line.substr(0, eq));
    std::string v = trim(line.substr(eq + 1));
    if (k == "product") out.product = v;
    else if (k == "expiry") {
      try {
        out.expiry_unix = std::stoull(v);
      } catch (...) {
        err = "bad expiry";
        return false;
      }
    } else if (k == "hwid")
      out.hwid = v;
    else if (k == "sig_b64")
      out.sig_b64 = v;
  }
  if (!head) {
    err = "missing BLCPP1";
    return false;
  }
  if (out.product.empty() || out.sig_b64.empty()) {
    err = "missing fields";
    return false;
  }
  return true;
}

bool LicenseVerifyToken(const std::string& token_b64_ascii, void* rsa_public_key_handle,
                        const std::string& current_hwid, std::string& err) {
  err.clear();
  std::vector<uint8_t> raw;
  if (!Base64Decode(token_b64_ascii, raw)) {
    err = "base64 decode failed";
    return false;
  }
  std::string inner(reinterpret_cast<const char*>(raw.data()), raw.size());
  LicenseFields f{};
  if (!LicenseParseInner(inner, f, err)) return false;

  if (f.hwid != "*" && f.hwid != current_hwid) {
    err = "machine mismatch";
    return false;
  }

  FILETIME ft{};
  GetSystemTimeAsFileTime(&ft);
  ULARGE_INTEGER uli{};
  uli.LowPart = ft.dwLowDateTime;
  uli.HighPart = ft.dwHighDateTime;
  uint64_t now_sec = (uli.QuadPart - 116444736000000000ULL) / 10000000ULL;
  if (now_sec > f.expiry_unix) {
    err = "expired";
    return false;
  }

  std::string canon = LicenseCanonicalUtf8(f.product, f.expiry_unix, f.hwid);
  std::vector<uint8_t> sig_raw;
  if (!Base64Decode(f.sig_b64, sig_raw)) {
    err = "sig base64";
    return false;
  }
  if (!RsaVerifySha256Pkcs1(rsa_public_key_handle,
                            reinterpret_cast<const uint8_t*>(canon.data()), canon.size(),
                            sig_raw.data(), sig_raw.size())) {
    err = "bad signature";
    return false;
  }
  return true;
}

bool LicenseBuildToken(const std::string& product, uint64_t expiry_unix, const std::string& hwid,
                       void* rsa_private_key_handle, std::string& token_b64_out,
                       std::string& err) {
  err.clear();
  token_b64_out.clear();
  std::string canon = LicenseCanonicalUtf8(product, expiry_unix, hwid);
  std::vector<uint8_t> sig;
  if (!RsaSignSha256Pkcs1(rsa_private_key_handle,
                         reinterpret_cast<const uint8_t*>(canon.data()), canon.size(), sig)) {
    err = "sign failed";
    return false;
  }
  std::string sig_b64;
  if (!Base64Encode(sig.data(), sig.size(), sig_b64)) {
    err = "b64 sig";
    return false;
  }
  std::string inner = LicensePackUtf8(product, expiry_unix, hwid, sig_b64);
  if (!Base64Encode(reinterpret_cast<const uint8_t*>(inner.data()), inner.size(), token_b64_out)) {
    err = "b64 token";
    return false;
  }
  return true;
}
