#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include "helpers.h"
#include "tag.h"
#include "package.h"
#include "globaldata.h"

#define _ITERATOR_DEBUG_LEVEL 2

const static unsigned char AES_KEY_0[16] =
{
	0xD6, 0x2A, 0xB2, 0xC1, 0x0C, 0xC0, 0x1B, 0xC5, 0x35, 0xDB, 0x7B, 0x86, 0x55, 0xC7, 0xDC, 0x3B,
};

const static unsigned char AES_KEY_1[16] =
{
	0x3A, 0x4A, 0x5D, 0x36, 0x73, 0xA6, 0x60, 0x58, 0x7E, 0x63, 0xE6, 0x76, 0xE4, 0x08, 0x92, 0xB5,
};

const int BLOCK_SIZE = 0x40000;

typedef int64_t(*OodleLZ64_DecompressDef)(unsigned char* Buffer, int64_t BufferSize, unsigned char* OutputBuffer, int64_t OutputBufferSize, int32_t a, int32_t b, int64_t c, void* d, void* e, void* f, void* g, void* h, void* i, int32_t ThreadModule);

void print_hex(const unsigned char* data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        printf("%02X", data[i]); // print each byte as two-digit hex
    }
    printf("\n");
}

std::vector<uint32_t> GetAllTagsFromReference(uint32_t reference) {
	std::cout << "Collect all function cast for - " << reference << std::endl;
	std::vector<uint32_t> AllTagHashesOfType;
    for (const auto& pair : GlobalData::getMap()) {
		int EntryID = 0;
        for (auto entry : pair.second.Entries) {
            if (entry.reference == reference) {
                int Num = (0x80800000 + (pair.second.Header.pkgID << 0xD) + EntryID);
                AllTagHashesOfType.push_back(Num);
		    }
            EntryID++;
        }
    }
    return AllTagHashesOfType;
}


void Block::print() const {
    std::cout << "Ref: " << offset
        << " Type: " << size
        << " Subtype: " << patchID
        << " bitFlag: " << bitFlag
        << " SHA1: ";

    for (int i = 0; i < 0x14; ++i)
        std::cout << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(SHA1[i]);

    std::cout << " gcmTag: ";
    for (int i = 0; i < 0x10; ++i)
        std::cout << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(gcmTag[i]);

    std::cout << std::dec << std::endl;  // Reset to decimal
}

bool Package::decryptBlock(Block block, unsigned char* blockBuffer, unsigned char*& decryptBuffer) {
    BCRYPT_ALG_HANDLE hAesAlg;
    NTSTATUS status;
	bool decryptionSuccess = true;
    status = BCryptOpenAlgorithmProvider(&hAesAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    status = BCryptSetProperty(hAesAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
        sizeof(BCRYPT_CHAIN_MODE_GCM), 0);

    alignas(alignof(BCRYPT_KEY_DATA_BLOB_HEADER)) unsigned char keyData[sizeof(BCRYPT_KEY_DATA_BLOB_HEADER) + 16];
    BCRYPT_KEY_DATA_BLOB_HEADER* pHeader = (BCRYPT_KEY_DATA_BLOB_HEADER*)keyData;
    pHeader->dwMagic = BCRYPT_KEY_DATA_BLOB_MAGIC;
    pHeader->dwVersion = BCRYPT_KEY_DATA_BLOB_VERSION1;
    pHeader->cbKeyData = 16;
    if (hasRedactedKey) {
        memcpy(pHeader + 1, redacted_key, 16);
    }
    else {
        memcpy(pHeader + 1, block.bitFlag & 0x4 ? AES_KEY_1 : AES_KEY_0, 16);
    }
    BCRYPT_KEY_HANDLE hAesKey;
    status = BCryptImportKey(hAesAlg, nullptr, BCRYPT_KEY_DATA_BLOB, &hAesKey, nullptr, 0, keyData, sizeof(keyData), 0);
    ULONG decryptionResult;
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO cipherModeInfo;

    BCRYPT_INIT_AUTH_MODE_INFO(cipherModeInfo);

    cipherModeInfo.pbTag = (PUCHAR)block.gcmTag;
    cipherModeInfo.cbTag = 0x10;
    if (hasRedactedKey) {
        cipherModeInfo.pbNonce = redacted_nonce;
    }
    else {
        cipherModeInfo.pbNonce = nonce;
    }
    cipherModeInfo.cbNonce = 0xC;
    
    status = BCryptDecrypt(hAesKey, (PUCHAR)blockBuffer, (ULONG)block.size, &cipherModeInfo, nullptr, 0,
        (PUCHAR)decryptBuffer, (ULONG)block.size, &decryptionResult, 0);
    if (status < 0) {// && status != -1073700862)
        printf("\nbcrypt decryption failed!");
        decryptionSuccess = false;
    }
    BCryptDestroyKey(hAesKey);
    BCryptCloseAlgorithmProvider(hAesAlg, 0);
    std::fill(&blockBuffer[0], &blockBuffer[block.size], 0);
    return decryptionSuccess;
}

void Package::ModifyNonce() {
    nonce[0] ^= (Header.pkgID >> 8) & 0xFF;
    nonce[11] ^= Header.pkgID & 0xFF;
}


ExtractResult Package::ExtractEntry(const int EntryNumber) {
	Entry entry = Entries[EntryNumber];
    ExtractResult returnResult;
    returnResult.success = true;
    unsigned char* fileBuffer = new unsigned char[entry.file_size];
    int currentBlockId = entry.starting_block;
    int startingBlockOffset = entry.starting_block_offset;
    int blockCount = floor((entry.starting_block_offset + entry.file_size - 1) / BLOCK_SIZE);
    int lastBlockID = currentBlockId + blockCount;
    int currentBufferOffset = 0;
    while (currentBlockId <= lastBlockID) {
        const Block& currentBlock = Blocks[currentBlockId];
        std::vector<unsigned char> realblockBuffer(currentBlock.size);
        unsigned char* blockBuffer = realblockBuffer.data();
        unsigned char* decryptBuffer = nullptr;
        std::vector<unsigned char> realDecryptBuffer(currentBlock.size);
        decryptBuffer = &realDecryptBuffer[0];
        unsigned char* decompBuffer = nullptr;
        std::vector<unsigned char> realDecompBuffer(BLOCK_SIZE);
        decompBuffer = &realDecompBuffer[0];
        std::string filename = PackageName + "_" + std::to_string(currentBlock.patchID) + ".pkg";
        FILE* pFile;
        pFile = _fsopen(filename.c_str(), "rb", _SH_DENYNO);
        fseek(pFile, currentBlock.offset, SEEK_SET);
        size_t result;
        result = fread(blockBuffer, 1, currentBlock.size, pFile);
        if (result != currentBlock.size) { fputs("Reading error", stderr); exit(3); }
        if (currentBlock.bitFlag & 0x2) {
            auto decryptionSuccess = decryptBlock(currentBlock, blockBuffer, decryptBuffer);  // Pass by reference
            if (decryptionSuccess == false) {
                returnResult.success = false;
            }
        }
        else {
            decryptBuffer = blockBuffer;
        }
        if (currentBlock.bitFlag & 0x1) {
            if (!initOodle()) {
                std::cerr << "Failed to initialize Oodle library!" << std::endl;
            }
            decompressBlock(currentBlock, decryptBuffer, decompBuffer);
        }
        else {
            decompBuffer = decryptBuffer;
        }
        if (currentBlockId == entry.starting_block)
        {
            size_t cpySize;
            if (currentBlockId == lastBlockID)
                cpySize = entry.file_size;
            else
                cpySize = BLOCK_SIZE - entry.starting_block_offset;
            memcpy(fileBuffer, decompBuffer + entry.starting_block_offset, cpySize);
            currentBufferOffset += cpySize;
        }
        else if (currentBlockId == lastBlockID)
        {
            memcpy(fileBuffer + currentBufferOffset, decompBuffer, entry.file_size - currentBufferOffset);
        }
        else
        {
            memcpy(fileBuffer + currentBufferOffset, decompBuffer, BLOCK_SIZE);
            currentBufferOffset += BLOCK_SIZE;
        }
        fclose(pFile);
        currentBlockId++;
        std::fill(&decompBuffer[0], &decompBuffer[sizeof(decompBuffer)], 0);
    }
	returnResult.data = fileBuffer;
    return returnResult;
}

void Package::decompressBlock(Block block, unsigned char* decryptBuffer, unsigned char*& decompBuffer)
{
    int64_t result = ((OodleLZ64_DecompressDef)OodleLZ_Decompress)(decryptBuffer, block.size, decompBuffer, BLOCK_SIZE, 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, 3);
    if (result <= 0)
        auto a = 0;
    //std::fill(&decryptBuffer[0], &decryptBuffer[block.size], 0);
}

bool Package::initOodle()  
{  
    size_t PatchPos = PackagePath.rfind("/");  
    std::string OodlePath = PackagePath.substr(0, PatchPos);  
    std::filesystem::path dllPath = std::filesystem::path(OodlePath) / "bin" / "x64" / "oo2core_9_win64.dll";
    HMODULE hOodleDll = LoadLibrary(dllPath.c_str());

    if (hOodleDll == nullptr) {  
        return false;  
    }  
    OodleLZ_Decompress = (int64_t)GetProcAddress(hOodleDll, "OodleLZ_Decompress");  
    if (!OodleLZ_Decompress) printf("Failed to find Oodle compress/decompress functions in DLL!");  
    return true;  
}

std::unordered_map<int, Package> GeneratePackageCache(std::unordered_map<int, Redacted_Key_Pair> Redacted_Keys) {
    std::unordered_map<int, int> PatchFinder;
    std::unordered_map<int, Package> PackageMap;
    std::unordered_map<int, std::string> PackageNames;
    std::vector<std::string> Files;
    for (const auto& entry : fs::directory_iterator(PackagePath)) {
        if (entry.is_regular_file()) {
            Files.push_back(entry.path().string());
        }
    }
    for (std::string Path : Files) {
        Package pkg;
        if (pkg.readHeader(Path)) {
            if (PatchFinder.contains(pkg.Header.pkgID)) {
                if (PatchFinder[pkg.Header.pkgID] < pkg.Header.patchID) {
                    PatchFinder[pkg.Header.pkgID] = pkg.Header.patchID;
                }
            }
            else {
                PatchFinder[pkg.Header.pkgID] = pkg.Header.patchID;
            }
            std::string PackageFileName;
            size_t NamePos = Path.rfind("\\");
            PackageFileName = Path.substr(NamePos + 1);
            size_t PatchPos = PackageFileName.rfind("_");
            PackageFileName = PackageFileName.substr(0, PatchPos);
            PackageNames[pkg.Header.pkgID] = PackageFileName;
        }
    }
    for (const auto& pair : PatchFinder) {
        Package pkg;
        pkg.load(PackagePath + "/" + PackageNames[pair.first] + "_" + std::to_string(pair.second) + ".pkg");
        if (Redacted_Keys.contains(pkg.Header.packageGroupId)) {
			memcpy(pkg.redacted_nonce, Redacted_Keys[pkg.Header.packageGroupId].nonce, 12);
            memcpy(pkg.redacted_key, Redacted_Keys[pkg.Header.packageGroupId].key, 16);
			pkg.hasRedactedKey = true;

        }
        PackageMap[pair.first] = pkg;
    }
    return PackageMap;
}

void hex32_to_bytes(const std::string& hex, unsigned char out[16]) {
    if (hex.size() != 32) {
        throw std::invalid_argument("Hex string must be exactly 32 characters.");
    }
    for (int i = 0; i < 16; ++i) {
        std::string byteStr = hex.substr(i * 2, 2);
        out[i] = static_cast<unsigned char>(std::stoul(byteStr, nullptr, 16));
    }
}
void hex24_to_bytes(const std::string& hex, unsigned char out[12]) {
    if (hex.size() != 24) {
        throw std::invalid_argument("Hex string must be exactly 24 characters.");
    }
    for (int i = 0; i < 12; ++i) {
        std::string byteStr = hex.substr(i * 2, 2);
        out[i] = static_cast<unsigned char>(std::stoul(byteStr, nullptr, 16));
    }
}


std::unordered_map<int, Redacted_Key_Pair> Read_Redacted_Keys()
{
    std::unordered_map<int, Redacted_Key_Pair> keys = {};
    std::ifstream file("keys.txt", std::ios::binary);
    if (!file) {
        std::cerr << "Key File Not Found\n";;
    }
	int KeyCount = 0;
    std::string line;
    while (std::getline(file, line)) {  // split by '\n'
        std::stringstream ss(line);
        std::string part;

        std::vector<std::string> tokens;
        while (std::getline(ss, part, ':')) {  // split by ':'
            tokens.push_back(part);
        }
       if (tokens.size() == 3) {
           Redacted_Key_Pair keyPair;
           unsigned char keyBytes[16];
           hex32_to_bytes(tokens[1], keyBytes);
           unsigned char ivBytes[12];
           hex24_to_bytes(tokens[2].substr(0,24), ivBytes);
           memcpy(keyPair.key, keyBytes, 16);
           memcpy(keyPair.nonce, ivBytes, 12);
           uint64_t PackageID = swapUInt64Endianness(hexStrToUint64(tokens[0]));
           keys.emplace(PackageID, keyPair);
           KeyCount++;
        }
    }
    printf("Loaded %d Redacted Keys\n", KeyCount);
    return keys;
}

