#include "rsa_cng.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

#include <fstream>
#include <vector>

static bool read_all_bin(const std::wstring& path, std::vector<uint8_t>& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  f.seekg(0, std::ios::end);
  auto sz = f.tellg();
  if (sz <= 0) return false;
  f.seekg(0, std::ios::beg);
  out.resize(static_cast<size_t>(sz));
  return static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()), sz));
}

static bool write_all_bin(const std::wstring& path, const uint8_t* data, size_t len) {
  std::ofstream f(path, std::ios::binary);
  if (!f) return false;
  return static_cast<bool>(f.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(len)));
}

bool Sha256Utf8(const std::string& utf8, std::vector<uint8_t>& out32) {
  BCRYPT_ALG_HANDLE hAlg = nullptr;
  NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
  if (!BCRYPT_SUCCESS(st)) return false;
  DWORD cb = 0, obj = 0;
  st = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&obj),
                         sizeof(obj), &cb, 0);
  if (!BCRYPT_SUCCESS(st)) {
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return false;
  }
  std::vector<uint8_t> objbuf(obj);
  BCRYPT_HASH_HANDLE hHash = nullptr;
  st = BCryptCreateHash(hAlg, &hHash, objbuf.data(), obj, nullptr, 0, 0);
  if (!BCRYPT_SUCCESS(st)) {
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return false;
  }
  if (!utf8.empty())
    st = BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<char*>(utf8.data())),
                        static_cast<ULONG>(utf8.size()), 0);
  if (!BCRYPT_SUCCESS(st)) {
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return false;
  }
  out32.resize(32);
  st = BCryptFinishHash(hHash, out32.data(), 32, 0);
  BCryptDestroyHash(hHash);
  BCryptCloseAlgorithmProvider(hAlg, 0);
  return BCRYPT_SUCCESS(st);
}

bool RsaGenerateKeyPairFiles(const std::wstring& dir, std::wstring& err) {
  err.clear();
  BCRYPT_ALG_HANDLE hAlg = nullptr;
  NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RSA_ALGORITHM, nullptr, 0);
  if (!BCRYPT_SUCCESS(st)) {
    err = L"BCryptOpenAlgorithmProvider RSA";
    return false;
  }
  BCRYPT_KEY_HANDLE hKey = nullptr;
  st = BCryptGenerateKeyPair(hAlg, &hKey, 2048, 0);
  if (!BCRYPT_SUCCESS(st)) {
    BCryptCloseAlgorithmProvider(hAlg, 0);
    err = L"BCryptGenerateKeyPair";
    return false;
  }
  st = BCryptFinalizeKeyPair(hKey, 0);
  if (!BCRYPT_SUCCESS(st)) {
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    err = L"BCryptFinalizeKeyPair";
    return false;
  }
  ULONG pubLen = 0;
  st = BCryptExportKey(hKey, nullptr, BCRYPT_RSAPUBLIC_BLOB, nullptr, 0, &pubLen, 0);
  if (!BCRYPT_SUCCESS(st)) {
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    err = L"BCryptExportKey pub size";
    return false;
  }
  std::vector<uint8_t> pub(pubLen);
  st = BCryptExportKey(hKey, nullptr, BCRYPT_RSAPUBLIC_BLOB, pub.data(), pubLen, &pubLen, 0);
  if (!BCRYPT_SUCCESS(st)) {
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    err = L"BCryptExportKey pub";
    return false;
  }
  ULONG prvLen = 0;
  st = BCryptExportKey(hKey, nullptr, BCRYPT_RSAFULLPRIVATE_BLOB, nullptr, 0, &prvLen, 0);
  if (!BCRYPT_SUCCESS(st)) {
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    err = L"BCryptExportKey prv size";
    return false;
  }
  std::vector<uint8_t> prv(prvLen);
  st = BCryptExportKey(hKey, nullptr, BCRYPT_RSAFULLPRIVATE_BLOB, prv.data(), prvLen, &prvLen, 0);
  BCryptDestroyKey(hKey);
  BCryptCloseAlgorithmProvider(hAlg, 0);
  if (!BCRYPT_SUCCESS(st)) {
    err = L"BCryptExportKey prv";
    return false;
  }
  std::wstring pubPath = dir + L"\\public.blb";
  std::wstring prvPath = dir + L"\\private.blb";
  if (!write_all_bin(pubPath, pub.data(), pub.size())) {
    err = L"write public.blb";
    return false;
  }
  if (!write_all_bin(prvPath, prv.data(), prv.size())) {
    err = L"write private.blb";
    return false;
  }
  return true;
}

static bool import_blob(bool is_private, const std::vector<uint8_t>& blob,
                        BCRYPT_ALG_HANDLE* phAlg, BCRYPT_KEY_HANDLE* phKey, std::wstring& err) {
  *phAlg = nullptr;
  *phKey = nullptr;
  NTSTATUS st = BCryptOpenAlgorithmProvider(phAlg, BCRYPT_RSA_ALGORITHM, nullptr, 0);
  if (!BCRYPT_SUCCESS(st)) {
    err = L"Open RSA alg";
    return false;
  }
  LPCWSTR name = is_private ? BCRYPT_RSAFULLPRIVATE_BLOB : BCRYPT_RSAPUBLIC_BLOB;
  st = BCryptImportKeyPair(*phAlg, nullptr, name, phKey, const_cast<PUCHAR>(blob.data()),
                           static_cast<ULONG>(blob.size()), 0);
  if (!BCRYPT_SUCCESS(st)) {
    BCryptCloseAlgorithmProvider(*phAlg, 0);
    *phAlg = nullptr;
    err = is_private ? L"Import private" : L"Import public";
    return false;
  }
  return true;
}

bool RsaLoadPublicKeyFromMemory(const uint8_t* data, size_t len, void** key_handle_out,
                                void** alg_handle_out, std::wstring& err) {
  if (!data || len < 32) {
    err = L"public blob too small";
    return false;
  }
  std::vector<uint8_t> blob(data, data + len);
  auto* alg = new BCRYPT_ALG_HANDLE(nullptr);
  auto* key = new BCRYPT_KEY_HANDLE(nullptr);
  if (!import_blob(false, blob, alg, key, err)) {
    delete alg;
    delete key;
    return false;
  }
  *alg_handle_out = alg;
  *key_handle_out = key;
  return true;
}

bool RsaLoadPublicKeyFromFile(const std::wstring& path, void** key_handle_out,
                              void** alg_handle_out, std::wstring& err) {
  std::vector<uint8_t> blob;
  if (!read_all_bin(path, blob)) {
    err = L"read public.blb";
    return false;
  }
  return RsaLoadPublicKeyFromMemory(blob.data(), blob.size(), key_handle_out, alg_handle_out, err);
}

bool RsaLoadPrivateKeyFromFile(const std::wstring& path, void** key_handle_out,
                               void** alg_handle_out, std::wstring& err) {
  std::vector<uint8_t> blob;
  if (!read_all_bin(path, blob)) {
    err = L"read private.blb";
    return false;
  }
  auto* alg = new BCRYPT_ALG_HANDLE(nullptr);
  auto* key = new BCRYPT_KEY_HANDLE(nullptr);
  if (!import_blob(true, blob, alg, key, err)) {
    delete alg;
    delete key;
    return false;
  }
  *alg_handle_out = alg;
  *key_handle_out = key;
  return true;
}

void RsaFreeKey(void* key_handle, void* alg_handle) {
  if (key_handle) {
    auto* k = static_cast<BCRYPT_KEY_HANDLE*>(key_handle);
    if (*k) BCryptDestroyKey(*k);
    delete k;
  }
  if (alg_handle) {
    auto* a = static_cast<BCRYPT_ALG_HANDLE*>(alg_handle);
    if (*a) BCryptCloseAlgorithmProvider(*a, 0);
    delete a;
  }
}

bool RsaSignSha256Pkcs1(void* key_handle, const uint8_t* msg, size_t msg_len,
                        std::vector<uint8_t>& signature) {
  auto* key = static_cast<BCRYPT_KEY_HANDLE*>(key_handle);
  std::vector<uint8_t> digest;
  if (!Sha256Utf8(std::string(reinterpret_cast<const char*>(msg), msg_len), digest))
    return false;
  BCRYPT_PKCS1_PADDING_INFO pad{};
  pad.pszAlgId = BCRYPT_SHA256_ALGORITHM;
  signature.resize(256);
  ULONG sigLen = static_cast<ULONG>(signature.size());
  NTSTATUS st = BCryptSignHash(*key, &pad, digest.data(), 32, signature.data(), sigLen,
                               &sigLen, BCRYPT_PAD_PKCS1);
  if (!BCRYPT_SUCCESS(st)) return false;
  signature.resize(sigLen);
  return true;
}

bool RsaVerifySha256Pkcs1(void* key_handle, const uint8_t* msg, size_t msg_len,
                          const uint8_t* sig, size_t sig_len) {
  auto* key = static_cast<BCRYPT_KEY_HANDLE*>(key_handle);
  std::vector<uint8_t> digest;
  std::string utf8(reinterpret_cast<const char*>(msg), msg_len);
  if (!Sha256Utf8(utf8, digest) || digest.size() != 32) return false;
  BCRYPT_PKCS1_PADDING_INFO pad{};
  pad.pszAlgId = BCRYPT_SHA256_ALGORITHM;
  NTSTATUS st =
      BCryptVerifySignature(*key, &pad, digest.data(), 32, const_cast<PUCHAR>(sig),
                            static_cast<ULONG>(sig_len), BCRYPT_PAD_PKCS1);
  return BCRYPT_SUCCESS(st);
}
