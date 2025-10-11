#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include "helpers.h"
#include "tag.h"
#include "package.h"
#include "globaldata.h"

#include <mutex>

using OodleLZ64_DecompressDef = int64_t(*)(unsigned char*, int64_t, unsigned char*, int64_t,
    int32_t, int32_t, int64_t, void*, void*, void*, void*, void*, void*, int32_t);

static std::once_flag               g_oodle_once;
static HMODULE                      g_oodle_dll = nullptr;
static OodleLZ64_DecompressDef      g_oodle_decomp = nullptr;

static std::once_flag    g_aes_once;
static BCRYPT_ALG_HANDLE g_aes_alg = nullptr;
static BCRYPT_KEY_HANDLE g_key0 = nullptr; // AES_KEY_0
static BCRYPT_KEY_HANDLE g_key1 = nullptr; // AES_KEY_1


const static unsigned char AES_KEY_0[16] =
{
	0xD6, 0x2A, 0xB2, 0xC1, 0x0C, 0xC0, 0x1B, 0xC5, 0x35, 0xDB, 0x7B, 0x86, 0x55, 0xC7, 0xDC, 0x3B,
};

const static unsigned char AES_KEY_1[16] =
{
	0x3A, 0x4A, 0x5D, 0x36, 0x73, 0xA6, 0x60, 0x58, 0x7E, 0x63, 0xE6, 0x76, 0xE4, 0x08, 0x92, 0xB5,
};

const int BLOCK_SIZE = 0x40000;

static BCRYPT_KEY_HANDLE ImportAesKey(BCRYPT_ALG_HANDLE alg, const unsigned char key[16]) {
    alignas(alignof(BCRYPT_KEY_DATA_BLOB_HEADER)) unsigned char blob[sizeof(BCRYPT_KEY_DATA_BLOB_HEADER) + 16];
    auto* hdr = reinterpret_cast<BCRYPT_KEY_DATA_BLOB_HEADER*>(blob);
    hdr->dwMagic = BCRYPT_KEY_DATA_BLOB_MAGIC;
    hdr->dwVersion = BCRYPT_KEY_DATA_BLOB_VERSION1;
    hdr->cbKeyData = 16;
    std::memcpy(hdr + 1, key, 16);

    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS st = BCryptImportKey(alg, nullptr, BCRYPT_KEY_DATA_BLOB, &hKey,
        nullptr, 0, blob, sizeof(blob), 0);
    return (st >= 0) ? hKey : nullptr;
}

#include <windows.h>

struct MappedPkg {
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMap = nullptr;
    const uint8_t* base = nullptr;
    size_t size = 0;

    bool open(const std::wstring& pathW) {
        hFile = CreateFileW(pathW.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER sz; if (!GetFileSizeEx(hFile, &sz)) return false;
        size = static_cast<size_t>(sz.QuadPart);
        hMap = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!hMap) return false;
        base = static_cast<const uint8_t*>(MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0));
        return base != nullptr;
    }
    const uint8_t* ptr(uint64_t off) const { return base + off; }

    ~MappedPkg() {
        if (base) UnmapViewOfFile(base);
        if (hMap) CloseHandle(hMap);
        if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    }
};


typedef int64_t(*OodleLZ64_DecompressDef)(unsigned char* Buffer, int64_t BufferSize, unsigned char* OutputBuffer, int64_t OutputBufferSize, int32_t a, int32_t b, int64_t c, void* d, void* e, void* f, void* g, void* h, void* i, int32_t ThreadModule);

void print_hex(const unsigned char* data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        printf("%02X", data[i]); // print each byte as two-digit hex
    }
    printf("\n");
}

static void InitAesOnce()
{
    if (BCryptOpenAlgorithmProvider(&g_aes_alg, BCRYPT_AES_ALGORITHM, nullptr, 0) < 0) return;
    (void)BCryptSetProperty(g_aes_alg, BCRYPT_CHAINING_MODE,
        (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
        (ULONG)sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    g_key0 = ImportAesKey(g_aes_alg, AES_KEY_0);
    g_key1 = ImportAesKey(g_aes_alg, AES_KEY_1);
}

// Per-thread duplicated handles (avoid lock contention inside CNG)
static thread_local BCRYPT_KEY_HANDLE tls_key0 = nullptr;
static thread_local BCRYPT_KEY_HANDLE tls_key1 = nullptr;
static thread_local BCRYPT_KEY_HANDLE tls_red = nullptr;

static void EnsureTlsKeys(class Package* pkg) {
    std::call_once(g_aes_once, InitAesOnce);
    if (!g_aes_alg) return;
    if (!tls_key0 && g_key0) BCryptDuplicateKey(g_key0, &tls_key0, nullptr, 0, 0);
    if (!tls_key1 && g_key1) BCryptDuplicateKey(g_key1, &tls_key1, nullptr, 0, 0);
    if (pkg && pkg->hasRedactedKey && pkg->hRedactedKey) {
        if (!tls_red) BCryptDuplicateKey(pkg->hRedactedKey, &tls_red, nullptr, 0, 0);
    }
}

std::vector<uint32_t> GetAllTagsFromReference(uint32_t reference) {
	std::cout << "Collect all function cast for - " << reference << std::endl;
	std::vector<uint32_t> AllTagHashesOfType;
    for (const auto& pair : GlobalData::getMap()) {
		int EntryID = 0;
        for (auto& entry : pair.second.Entries) {
            if (entry.reference == reference) {
                int Num = (0x80800000 + (pair.second.Header.pkgID << 0xD) + EntryID);
                AllTagHashesOfType.push_back(Num);
		    }
            EntryID++;
        }
    }
    printf("Finished Collecting for type %08X \n", reference);
    return AllTagHashesOfType;
}

std::vector<std::pair<uint32_t, const Package*>> GetAllTagsOfType(uint16_t type) {
    std::cout << "Collect all type function cast for - " << type << std::endl;

    std::vector<std::pair<uint32_t, const Package*>> out;

    // IMPORTANT: get a reference to the actual object in the map
    for (const auto& kv : GlobalData::getMap()) {
        const Package& pkg = kv.second;            // reference (NO copy)
        size_t entryID = 0;

        for (const auto& entry : pkg.Entries) {    // const is fine
            if (entry.file_type == type) {
                const uint32_t num = 0x80800000u
                    + (static_cast<uint32_t>(pkg.Header.pkgID) << 13)
                    + static_cast<uint32_t>(entryID);
                out.emplace_back(num, &pkg);       // pointer to map-resident object
            }
            ++entryID;
        }
    }
    return out;
}
bool Package::initOodle()
{
    std::call_once(g_oodle_once, [] {
        // Build: <packages root>/bin/x64/oo2core_9_win64.dll
        std::filesystem::path root = std::filesystem::path(PackagePath).parent_path();  // up from /packages
        std::filesystem::path dll = root / "bin" / "x64" / "oo2core_9_win64.dll";

        g_oodle_dll = LoadLibraryW(dll.wstring().c_str());
        if (!g_oodle_dll) return; // stays nullptr; initOodle() will report failure

        g_oodle_decomp = reinterpret_cast<OodleLZ64_DecompressDef>(
            GetProcAddress(g_oodle_dll, "OodleLZ_Decompress"));
        });

    // cache on the instance too (optional; avoids branching elsewhere)
    hOodleDll = g_oodle_dll;
    OodleLZ_Decompress = reinterpret_cast<int64_t>(g_oodle_decomp);

    return g_oodle_dll && g_oodle_decomp;
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

bool Package::decryptBlock(const Block block, unsigned char* ioBuffer /*in-place*/, unsigned char*& decryptBuffer /*ignored*/)
{
    decryptBuffer = ioBuffer;

    EnsureTlsKeys(this);
    BCRYPT_KEY_HANDLE hKey = nullptr;

    if (hasRedactedKey) {
        // Ensure package key imported once; then TLS-dupe
        if (!hRedactedKey && g_aes_alg)
            hRedactedKey = ImportAesKey(g_aes_alg, redacted_key);
        EnsureTlsKeys(this);
        hKey = tls_red ? tls_red : hRedactedKey;
    }
    else {
        hKey = (block.bitFlag & 0x4) ? tls_key1 : tls_key0;
    }

    if (!hKey) { std::fprintf(stderr, "AES key handle missing\n"); return false; }

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbTag = (PUCHAR)block.gcmTag;
    info.cbTag = 0x10;
    info.pbNonce = hasRedactedKey ? redacted_nonce : nonce;
    info.cbNonce = 0x0C;

    ULONG outSize = 0;
    NTSTATUS st = BCryptDecrypt(hKey,
        ioBuffer, (ULONG)block.size,
        &info,
        nullptr, 0,
        ioBuffer, (ULONG)block.size,  // in-place
        &outSize,
        0);
    if (st < 0) {
        std::fprintf(stderr, "BCryptDecrypt failed 0x%08X\n", (unsigned)st);
        return false;
    }
    return true;
}

void Package::ModifyNonce() {
    nonce[0] ^= (Header.pkgID >> 8) & 0xFF;
    nonce[11] ^= Header.pkgID & 0xFF;
}


ExtractResult Package::ExtractEntry(const int EntryNumber) {
    const Entry entry = Entries[EntryNumber];
    ExtractResult ret; ret.success = true;
    unsigned char* fileBuffer = new unsigned char[entry.file_size];
    ret.data = fileBuffer;
    int currentBlockId = entry.starting_block;
    const int blockCount = (int)std::floor((entry.starting_block_offset + entry.file_size - 1) / double(BLOCK_SIZE));
    const int lastBlockID = currentBlockId + blockCount;

    bool needsOodle = false, needsOtherPkg = false;
    {
        uint32_t firstBlock = entry.starting_block;
        uint32_t blocksNeeded = (uint32_t)((entry.starting_block_offset + entry.file_size + BLOCK_SIZE - 1) / BLOCK_SIZE);
        uint32_t lastBlock = firstBlock + blocksNeeded - 1;
        for (uint32_t b = firstBlock; b <= lastBlock; ++b) {
            needsOodle |= (Blocks[b].bitFlag & 0x1);
            needsOtherPkg |= (Blocks[b].patchID != Header.patchID);
            if (needsOodle && needsOtherPkg) break;
        }
    }
    if (needsOodle && !initOodle()) {
        std::fprintf(stderr, "Failed to initialize Oodle\n");
        delete[] fileBuffer; return { nullptr, false };
    }

    // ---- Reused buffers (no per-block allocs) ----
    std::vector<unsigned char> blockBuf;
    std::vector<unsigned char> decompBuf(BLOCK_SIZE);

    // ---- Reuse FILE* per patchID ----
    FILE* pFileGlobal = nullptr;
    std::unordered_map<uint32_t, FILE*> patchFiles;

    auto getFile = [&](uint32_t patchID) -> FILE* {
        if (!needsOtherPkg) return pFileGlobal;
        auto it = patchFiles.find(patchID);
        if (it != patchFiles.end()) return it->second;
        const std::string filename = PackageName + "_" + std::to_string(patchID) + ".pkg";
        FILE* f = _fsopen(filename.c_str(), "rb", _SH_DENYNO);
        if (!f) { std::fprintf(stderr, "Failed to open %s\n", filename.c_str()); return (FILE*)nullptr; }
        patchFiles.emplace(patchID, f);
        return f;
        };

    if (!needsOtherPkg) {
        const std::string filename = PackageName + "_" + std::to_string(Header.patchID) + ".pkg";
        pFileGlobal = _fsopen(filename.c_str(), "rb", _SH_DENYNO);
        if (!pFileGlobal) {
            std::fprintf(stderr, "Failed to open %s\n", filename.c_str());
            delete[] fileBuffer; return { nullptr, false };
        }
    }

    size_t currentBufferOffset = 0;

    while (currentBlockId <= lastBlockID) {
        const Block& blk = Blocks[currentBlockId];

        // ensure buffer capacity once per block size
        if (blockBuf.size() < blk.size) blockBuf.resize(blk.size);
        unsigned char* io = blockBuf.data();

        FILE* f = getFile(blk.patchID);
        if (!f) { ret.success = false; break; }

        if (_fseeki64(f, static_cast<long long>(blk.offset), SEEK_SET) != 0) {
            std::fputs("Seek error\n", stderr); ret.success = false; break;
        }
        size_t got = std::fread(io, 1, blk.size, f);
        if (got != blk.size) { std::fputs("Read error\n", stderr); ret.success = false; break; }

        // decrypt in-place if needed
        unsigned char* decryptPtr = io; // API needs it, but we keep it in-place
        if (blk.bitFlag & 0x2) {
            if (!decryptBlock(blk, io, decryptPtr)) { ret.success = false; break; }
        }

        // decompress if needed
        unsigned char* out = decryptPtr;
        if (blk.bitFlag & 0x1) {
            unsigned char* de = decompBuf.data();
            decompressBlock(blk, decryptPtr, de);
            out = de;
        }

        // stitch into final buffer
        if (currentBlockId == entry.starting_block) {
            const size_t cpy = (currentBlockId == lastBlockID)
                ? entry.file_size
                : (BLOCK_SIZE - entry.starting_block_offset);
            std::memcpy(fileBuffer, out + entry.starting_block_offset, cpy);
            currentBufferOffset += cpy;
        }
        else if (currentBlockId == lastBlockID) {
            std::memcpy(fileBuffer + currentBufferOffset, out, entry.file_size - currentBufferOffset);
        }
        else {
            std::memcpy(fileBuffer + currentBufferOffset, out, BLOCK_SIZE);
            currentBufferOffset += BLOCK_SIZE;
        }

        ++currentBlockId;
    }

    if (pFileGlobal) std::fclose(pFileGlobal);
    for (auto& kv : patchFiles) if (kv.second) std::fclose(kv.second);

    return ret;
}

void Package::decompressBlock(Block block, unsigned char* decryptBuffer, unsigned char*& decompBuffer)
{
    auto decompress = g_oodle_decomp
        ? g_oodle_decomp
        : reinterpret_cast<OodleLZ64_DecompressDef>(OodleLZ_Decompress);

    int64_t result = decompress(decryptBuffer, block.size, decompBuffer, BLOCK_SIZE,
        0, 0, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 3);
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
            std::memcpy(pkg.redacted_nonce, Redacted_Keys[pkg.Header.packageGroupId].nonce, 12);
            std::memcpy(pkg.redacted_key, Redacted_Keys[pkg.Header.packageGroupId].key, 16);
            pkg.hasRedactedKey = true;

            std::call_once(g_aes_once, InitAesOnce);
            if (g_aes_alg)
                pkg.hRedactedKey = ImportAesKey(g_aes_alg, pkg.redacted_key); // <--- import ONCE
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

static inline bool SearchTag(uint32_t tag, uint32_t value, const Package* pkg)
{
    TagHash blob(tag,true);
    auto p = blob.getDatawithPkg(pkg);
    std::size_t n = blob.size;
    if (!p || n < 4) return false;

    const unsigned char b0 = static_cast<unsigned char>(value & 0xFF);
    const unsigned char* cur = p;
    const unsigned char* end = p + n - 4 + 1;
    while (cur < end) {
        const void* hit = std::memchr(cur, b0, static_cast<size_t>(end - cur));
        if (!hit) return false;
        const unsigned char* f = static_cast<const unsigned char*>(hit);

        uint32_t u;
        std::memcpy(&u, f, sizeof(u));
        if (u == value) return true;

        cur = f + 1;
    }
    return false;
}

struct TagView {
    uint32_t tag;
    const unsigned char* data;
    size_t size;
};

int GetPkgID(uint32_t hash)
{
    uint16_t pkgID = floor((hash - 0x80800000) / 8192);
    return pkgID;
}

int getEntryID(uint32_t hash) {
    uint32_t entryId = hash & 0x1FFF;
    return entryId;
}

void SearchBungieFiles(uint32_t value)
{
    auto start = std::chrono::high_resolution_clock::now();
    auto hashes = GetAllTagsOfType(8);
    auto hashes16 = GetAllTagsOfType(16);
    for (auto hash : hashes16)
    {
        hashes.push_back(hash);
    }
    std::printf("Collected %zu Hashes for Type %d\n", hashes.size(), 8);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::printf("Collected Tags in %.3f seconds\n", elapsed.count());

    start = std::chrono::high_resolution_clock::now();

    std::vector<uint8_t> found(hashes.size());
    std::transform(std::execution::par_unseq,
        hashes.begin(), hashes.end(),
        found.begin(),
        [value](const std::pair<uint32_t, const Package*>& p) -> uint8_t {
            return SearchTag(p.first, value, p.second) ? 1u : 0u;
        });

    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;

    // gather results
    std::vector<uint32_t> matchingTags;
    for (std::size_t i = 0; i < hashes.size(); ++i) {
        if (found[i]) {
            matchingTags.push_back(hashes[i].first);
        }
    }

    double tagsPerSecond = hashes.empty() ? 0.0 : hashes.size() / elapsed.count();

    std::printf("Found value in %zu tags\n", matchingTags.size());
    std::printf("Processed %zu tags in %.3f seconds (~ %.1f tags/sec)\n",
        hashes.size(), elapsed.count(), tagsPerSecond);
    auto& pkgCache = GlobalData::getMap();
    if (!matchingTags.empty()) {
        std::printf("Matching tag IDs:\n");
        for (auto tag : matchingTags) {
            auto pname = pkgCache[GetPkgID(tag)];
            std::string filepath = pname.PackageName;
            std::string filename = fs::path(filepath).filename().string();
            std::printf("  %08x - %08x  in pkg - %s \n", tag, pname.Entries[getEntryID(tag)].reference, filename.c_str());
        }
    }
}