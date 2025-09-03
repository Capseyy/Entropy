#pragma once
#include <string>
#include <unordered_map>
#include "string.h"
#include "TigerEngine/package.h"
#include <future>
#include "TigerEngine/tag.h"
#include <iostream>
#include <codecvt>
#include <locale>


std::unordered_map<int, std::string> GenerateStringMap();
std::unordered_map<int, std::string> ProcessStringContainer(int Hash);

struct Unk_0x70008080 {
    uint32_t hash{};
};

struct SStringContainer {
public:
    uint64_t FileSize{};
    std::vector<Unk_0x70008080> HashTable;
    TagHash Language{};
};

struct RawStringCharacter {
	uint8_t character{};
};

struct SStringPart {
public:
	uint64_t Unk00{};
	RelativePointer64 StringOffset{};
	uint32_t Unk10{};
	uint16_t ByteLength{};
	uint16_t StringLength{};
	uint64_t Unk18{};
};

struct SStringMetadata {
	RelativePointer64 partOffset{};
	uint64_t partCount{};
};


struct SStringBank {
public:
    uint64_t FileSize{};
	std::vector<SStringPart> StringParts;
	std::array<uint32_t,8> Unk18;
	std::vector<SStringMetadata> StringMetadata;
};

