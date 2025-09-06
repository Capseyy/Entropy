#pragma once
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>


uint16_t swapUInt16Endianness(uint16_t x);
uint32_t swapUInt32Endianness(uint32_t x);
uint64_t swapUInt64Endianness(uint64_t x);
uint32_t hexStrToUint16(std::string hash);
uint32_t hexStrToUint32(std::string hash);
uint64_t hexStrToUint64(std::string hash);
void print_hex_dump(const unsigned char* data, std::size_t size);