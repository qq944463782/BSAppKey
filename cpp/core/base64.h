#pragma once

#include <cstdint>
#include <string>
#include <vector>

bool Base64Encode(const uint8_t* data, size_t len, std::string& out_ascii);
bool Base64Decode(const std::string& ascii, std::vector<uint8_t>& out);
