#include "base64.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>

#pragma comment(lib, "crypt32.lib")

bool Base64Encode(const uint8_t* data, size_t len, std::string& out_ascii) {
  if (!data && len > 0) return false;
  DWORD flags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;
  DWORD need = 0;
  if (!CryptBinaryToStringA(data, static_cast<DWORD>(len), flags, nullptr, &need))
    return false;
  std::string buf(static_cast<size_t>(need), '\0');
  if (!CryptBinaryToStringA(data, static_cast<DWORD>(len), flags, buf.data(), &need))
    return false;
  while (!buf.empty() && buf.back() == '\0') buf.pop_back();
  out_ascii = std::move(buf);
  return true;
}

bool Base64Decode(const std::string& ascii, std::vector<uint8_t>& out) {
  DWORD skip = 0;
  DWORD flags = CRYPT_STRING_BASE64;
  DWORD need = 0;
  if (!CryptStringToBinaryA(ascii.c_str(), static_cast<DWORD>(ascii.size()), flags,
                            nullptr, &need, &skip, nullptr))
    return false;
  out.resize(need);
  if (!CryptStringToBinaryA(ascii.c_str(), static_cast<DWORD>(ascii.size()), flags,
                            out.data(), &need, &skip, nullptr))
    return false;
  out.resize(need);
  return true;
}
