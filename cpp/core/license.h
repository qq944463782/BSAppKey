#pragma once

#include <cstdint>
#include <string>

/** Build canonical UTF-8 message that is signed. */
std::string LicenseCanonicalUtf8(const std::string& product, uint64_t expiry_unix,
                                 const std::string& hwid);

/** Full license block UTF-8 (before outer base64). */
std::string LicensePackUtf8(const std::string& product, uint64_t expiry_unix,
                            const std::string& hwid, const std::string& sig_b64_ascii);

struct LicenseFields {
  std::string product;
  uint64_t expiry_unix = 0;
  std::string hwid;
  std::string sig_b64;
};

/** Parse inner license text after outer base64 decode. */
bool LicenseParseInner(const std::string& inner_utf8, LicenseFields& out, std::string& err);

/** err empty => OK */
bool LicenseVerifyToken(const std::string& token_b64_ascii, void* rsa_public_key_handle,
                        const std::string& current_hwid, std::string& err);

bool LicenseBuildToken(const std::string& product, uint64_t expiry_unix, const std::string& hwid,
                       void* rsa_private_key_handle, std::string& token_b64_out,
                       std::string& err);
