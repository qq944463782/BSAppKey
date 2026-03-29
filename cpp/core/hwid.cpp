#include "hwid.h"
#include "rsa_cng.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <vector>

static std::string WideToUtf8(std::wstring_view w) {
  if (w.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0,
                              nullptr, nullptr);
  if (n <= 0) return {};
  std::string s(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr,
                      nullptr);
  return s;
}

std::string HwidFingerprintUtf8() {
  HKEY hkey = nullptr;
  std::wstring guid;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0,
                    KEY_READ | KEY_WOW64_64KEY, &hkey) != ERROR_SUCCESS)
    return {};
  wchar_t buf[256]{};
  DWORD sz = sizeof(buf);
  DWORD type = 0;
  LSTATUS st = RegQueryValueExW(hkey, L"MachineGuid", nullptr, &type,
                                reinterpret_cast<BYTE*>(buf), &sz);
  RegCloseKey(hkey);
  if (st != ERROR_SUCCESS || type != REG_SZ) return {};
  guid.assign(buf);
  std::transform(guid.begin(), guid.end(), guid.begin(), ::towlower);
  std::string raw = WideToUtf8(guid);
  if (raw.empty()) return {};

  std::vector<uint8_t> hash;
  if (!Sha256Utf8(raw, hash) || hash.size() != 32) return {};

  static const char* hx = "0123456789abcdef";
  std::string out;
  out.resize(64);
  for (size_t i = 0; i < 32; ++i) {
    out[i * 2] = hx[(hash[i] >> 4) & 0xF];
    out[i * 2 + 1] = hx[hash[i] & 0xF];
  }
  return out;
}
