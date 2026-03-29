#pragma once

#include <cstdint>
#include <string>
#include <vector>

/** SHA-256 of UTF-8 string (binary digest). */
bool Sha256Utf8(const std::string& utf8, std::vector<uint8_t>& out32);

bool RsaGenerateKeyPairFiles(const std::wstring& dir, std::wstring& err);
bool RsaLoadPublicKeyFromFile(const std::wstring& path, void** key_handle_out,
                              void** alg_handle_out, std::wstring& err);
/** CNG BCRYPT_RSAPUBLIC_BLOB bytes (e.g. from public.blb). */
bool RsaLoadPublicKeyFromMemory(const uint8_t* data, size_t len, void** key_handle_out,
                                void** alg_handle_out, std::wstring& err);
bool RsaLoadPrivateKeyFromFile(const std::wstring& path, void** key_handle_out,
                               void** alg_handle_out, std::wstring& err);
void RsaFreeKey(void* key_handle, void* alg_handle);

bool RsaSignSha256Pkcs1(void* key_handle, const uint8_t* msg, size_t msg_len,
                        std::vector<uint8_t>& signature);
bool RsaVerifySha256Pkcs1(void* key_handle, const uint8_t* msg, size_t msg_len,
                          const uint8_t* sig, size_t sig_len);
