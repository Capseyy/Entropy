#pragma once
#pragma comment(lib, "bcrypt.lib")
#define PACKAGE_H
#include <string>
#include <vector>
#include <array>
#include <windows.h>
#include <stdlib.h>
#include <filesystem>
#include <bcrypt.h>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <cstdint>
#include "TigerEngine/tag.h"

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct EntryHeaderRaw {
    uint32_t reference;
    uint32_t _type_info;
    uint64_t _block_info;
};
#pragma pack(pop)

struct Hash64
{
    uint64_t hash64;
    uint32_t hash32;
    uint32_t reference;

    void print(const Hash64& h) {
        std::cout << "---- H64 Entry ----" << std::endl;
        std::cout << std::hex << std::setfill('0');
        std::cout << "Hash64   " << std::setw(16) << h.hash64 << std::endl;
        std::cout << "Hash32   " << std::setw(8) << h.hash32 << std::endl;
        std::cout << "Reference   " << std::setw(8) << h.reference << std::endl;
        std::cout << std::dec;
    }
};

struct Entry
{
    uint32_t reference;
    uint32_t _type_info;
    uint64_t _block_info;

    uint8_t file_type;
    uint8_t file_subtype;
    uint32_t starting_block;
    uint32_t starting_block_offset;
    uint32_t file_size;

    Entry(const EntryHeaderRaw& raw)
        : reference(raw.reference), _type_info(raw._type_info), _block_info(raw._block_info)
    {
        file_type = static_cast<uint8_t>((_type_info >> 9) & 0x7F);
        file_subtype = static_cast<uint8_t>((_type_info >> 6) & 0x07);
        starting_block = static_cast<uint32_t>(_block_info & 0x3FFF);
        starting_block_offset = static_cast<uint32_t>(((_block_info >> 14) & 0x3FFF) << 4);
        file_size = static_cast<uint32_t>(_block_info >> 28);
    }
    void print() {
        std::cout << "Ref: " << reference
            << " Type: " << +file_type
            << " Subtype: " << +file_subtype
            << " starting_block: " << +starting_block
            << " starting_block_offset: " << +starting_block_offset
            << " Size: " << file_size << std::endl;
    }
};

struct PkgHeader
{
    uint8_t _pad0[8];
    uint64_t packageGroupId;
    uint16_t pkgID;
    uint8_t _0x10[30];
    uint16_t patchID;
    uint8_t _0x32[46];
    uint32_t entryTableSize;
    uint32_t entryTableOffset;
    uint32_t blockTableSize;
    uint32_t blockTableOffset;
    uint8_t _pad2[8];
    uint32_t namedTagCount;
    uint32_t namedTagOffset;
    uint8_t _pad3[56];
    uint32_t hash64TableSize; //0xb8
    uint32_t hash64TableOffset; //0xbc


    void printHeader(const PkgHeader& h) {
        std::cout << "---- PkgHeader ----" << std::endl;
        std::cout << "Package Group ID:   " << h.packageGroupId << std::endl;
        std::cout << "Package ID:         " << h.pkgID << std::endl;
        std::cout << "Patch ID:           " << h.patchID << std::endl;

        std::cout << "Entry Table Size:   " << h.entryTableSize << std::endl;
        std::cout << "Entry Table Offset: " << h.entryTableOffset << std::endl;
        std::cout << "Block Table Size:   " << h.blockTableSize << std::endl;
        std::cout << "Block Table Offset: " << h.blockTableOffset << std::endl;

        std::cout << "Named Tag Count:    " << h.namedTagCount << std::endl;
        std::cout << "Named Tag Offset:   " << h.namedTagOffset << std::endl;

        std::cout << "Hash64 Table Size:  " << h.hash64TableSize << std::endl;
        std::cout << "Hash64 Table Offset:" << h.hash64TableOffset << std::endl;
        std::cout << "--------------------" << std::endl;
    }


};



struct Block
{
    uint32_t offset;
    uint32_t size;
    uint16_t patchID;
    uint16_t bitFlag;
    uint8_t SHA1[0x14];
    uint8_t gcmTag[0x10];

    void print() const;
};


struct ExtractResult {
    unsigned char* data;
    bool success;
};

class Package {
public:
    std::vector<Block> Blocks;
    std::vector<Entry> Entries;
    std::vector<Hash64> h64s;
    PkgHeader Header;
    std::string PackageName;
    unsigned char nonce[12] =
    {
        0x84, 0xEA, 0x11, 0xC0, 0xAC, 0xAB, 0xFA, 0x20, 0x33, 0x11, 0x26, 0x99,
    };
    int64_t OodleLZ_Decompress;
    HMODULE hOodleDll;
    unsigned char redacted_key[16];
    unsigned char redacted_nonce[12];
    bool hasRedactedKey = false;
    bool load(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open file\n";
            return false;
        }
        file.read(reinterpret_cast<char*>(&Header), sizeof(PkgHeader));
        std::string PackageFileName;
        size_t NamePos = filepath.rfind("\\");
        PackageFileName = filepath.substr(NamePos + 1);
        size_t PatchPos = PackageFileName.rfind("_");
        PackageName = PackageFileName.substr(0, PatchPos);
        file.seekg(Header.entryTableOffset, std::ios::beg);
        for (int i = 0; i < Header.entryTableSize; i++) {
            EntryHeaderRaw rawHeader;
            file.read(reinterpret_cast<char*>(&rawHeader), sizeof(EntryHeaderRaw));
            Entries.emplace_back(rawHeader);
        }
        file.seekg(Header.blockTableOffset, std::ios::beg);
        for (int i = 0; i < Header.blockTableSize; i++) {
            Block block;
            file.read(reinterpret_cast<char*>(&block), sizeof(block));
            Blocks.push_back(block);
            //block.print();
        }
        if (!Header.hash64TableSize == 0) {
            file.seekg(Header.hash64TableOffset + 0x60, std::ios::beg);
            for (int i = 0; i < Header.hash64TableSize; i++) {
                Hash64 h64;
                file.read(reinterpret_cast<char*>(&h64), sizeof(Hash64));
                h64s.emplace_back(h64);
            }
        }

        ModifyNonce();
        //printf("Package loaded: %s\n", PackageName.c_str());
        return true;
    }

    bool readHeader(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open file\n";
            return false;
        }
        file.read(reinterpret_cast<char*>(&Header), sizeof(PkgHeader));
        return true;
    }
    bool decryptBlock(Block block, unsigned char* blockBuffer, unsigned char*& decryptBuffer);
    void ModifyNonce();
    bool initOodle();
    void decompressBlock(Block block, unsigned char* decryptBuffer, unsigned char*& decompBuffer);
    ExtractResult ExtractEntry(int EntryNumber);
};


const std::string PackagePath = "C:/Program Files (x86)/Steam/steamapps/common/Destiny 2/packages";


#pragma once

struct Redacted_Key_Pair {
    unsigned char key[16];
    unsigned char nonce[12];
};

std::unordered_map<int, Package> GeneratePackageCache(std::unordered_map<int, Redacted_Key_Pair> Redacted_Keys);

std::unordered_map<int, Redacted_Key_Pair> Read_Redacted_Keys();

std::vector<uint32_t> GetAllTagsFromReference(uint32_t reference);
