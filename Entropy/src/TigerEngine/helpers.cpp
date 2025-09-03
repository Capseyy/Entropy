#include "helpers.h"

void print_hex_dump(const unsigned char* data, std::size_t size) {
	size_t width = 16; // Number of bytes per line
	for (std::size_t i = 0; i < size; i += width) {
		// print offset
		std::cout << std::setw(8) << std::setfill('0') << std::hex << i << "  ";

		// hex part
		for (std::size_t j = 0; j < width; ++j) {
			if (i + j < size)
				std::cout << std::setw(2) << static_cast<unsigned>(data[i + j]) << ' ';
			else
				std::cout << "   ";
		}

		// ASCII part
		std::cout << " |";
		for (std::size_t j = 0; j < width && i + j < size; ++j) {
			unsigned char c = data[i + j];
			std::cout << (std::isprint(c) ? static_cast<char>(c) : '.');
		}
		std::cout << "|\n";
	}
}

uint16_t swapUInt16Endianness(uint16_t x)
{
	x = (x << 8) + (x >> 8);
	return x;
}

uint32_t swapUInt32Endianness(uint32_t x)
{
	x = (x >> 24) |
		((x << 8) & 0x00FF0000) |
		((x >> 8) & 0x0000FF00) |
		(x << 24);
	return x;
}

uint64_t swapUInt64Endianness(uint64_t k)
{
	return ((k << 56) |
		((k & 0x000000000000FF00) << 40) |
		((k & 0x0000000000FF0000) << 24) |
		((k & 0x00000000FF000000) << 8) |
		((k & 0x000000FF00000000) >> 8) |
		((k & 0x0000FF0000000000) >> 24) |
		((k & 0x00FF000000000000) >> 40) |
		(k >> 56)
		);
}

uint32_t hexStrToUint16(std::string hash)
{
	return swapUInt16Endianness(std::stoul(hash, nullptr, 16));
}

uint32_t hexStrToUint32(std::string hash)
{
	return swapUInt32Endianness(std::stoul(hash, nullptr, 16));
}

uint64_t hexStrToUint64(std::string hash)
{
	return swapUInt64Endianness(std::stoull(hash, nullptr, 16));
}