#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include "helpers.h"
#include "tag.h"
#include "package.h"
#include "globaldata.h"
#undef min
#include <mutex>

using OodleLZ64_DecompressDef = int64_t(*)(unsigned char*, int64_t, unsigned char*, int64_t,
    int32_t, int32_t, int64_t, void*, void*, void*, void*, void*, void*, int32_t);

static std::once_flag               g_oodle_once;
static HMODULE                      g_oodle_dll = nullptr;
static OodleLZ64_DecompressDef      g_oodle_decomp = nullptr;

static std::once_flag    g_aes_once;

constexpr size_t BLOCK_SIZE = 0x40000u;


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

    const unsigned char* key = hasRedactedKey ? redacted_key : (block.bitFlag & 0x4) ? AES_KEY_1 : AES_KEY_0;
    const unsigned char* iv = hasRedactedKey ? redacted_nonce : nonce;
    const unsigned char* tag = block.gcmTag;

    if (!AESGCM_Decrypt(key, iv, tag, ioBuffer, block.size, PackageName)) {
        return false;
    }
    return true;
}

static std::mutex g_mapMux;
struct MappedRec { std::unique_ptr<MappedPkg> mp; };
static std::unordered_map<std::string, MappedRec> g_mapped;

static const MappedPkg* MapPkgOnce(const std::string& fullPathUtf8) {
    if (auto it = g_mapped.find(fullPathUtf8); it != g_mapped.end())
        return it->second.mp.get();

    std::lock_guard<std::mutex> lk(g_mapMux);
    if (auto it = g_mapped.find(fullPathUtf8); it != g_mapped.end())
        return it->second.mp.get();

    auto rec = MappedRec{};
    rec.mp = std::make_unique<MappedPkg>();
    std::filesystem::path p(fullPathUtf8);
    if (!rec.mp->open(p.wstring())) return nullptr;

#if _WIN32_WINNT >= 0x0602
    WIN32_MEMORY_RANGE_ENTRY r{ (PVOID)rec.mp->base, (SIZE_T)rec.mp->size };
    PrefetchVirtualMemory(GetCurrentProcess(), 1, &r, 0);
#endif
    const MappedPkg* out = rec.mp.get();
    g_mapped.emplace(fullPathUtf8, std::move(rec));
    return out;
}

void Package::ModifyNonce() {
    nonce[0] ^= (Header.pkgID >> 8) & 0xFF;
    nonce[11] ^= Header.pkgID & 0xFF;
}

ExtractResult Package::ExtractEntry(const int EntryNumber)
{
    const Entry entry = Entries[EntryNumber];
    ExtractResult ret;
    ret.success = false;

    // ---- allocate destination ----
    unsigned char* fileBuffer = new unsigned char[entry.file_size];
    ret.data = fileBuffer;

    // ---- block range ----
    const int firstBlock = entry.starting_block;
    const int blocksNeeded =
        static_cast<int>((entry.starting_block_offset + entry.file_size + BLOCK_SIZE - 1) / BLOCK_SIZE);
    const int lastBlock = firstBlock + blocksNeeded - 1;

    // ---- detect features ----
    bool needsOodle = false, needsOtherPkg = false;
    for (int b = firstBlock; b <= lastBlock; ++b) {
        needsOodle |= (Blocks[b].bitFlag & 0x1);
        needsOtherPkg |= (Blocks[b].patchID != Header.patchID);
        if (needsOodle && needsOtherPkg) break;
    }
    if (needsOodle && !initOodle()) {
        std::fprintf(stderr, "Failed to initialize Oodle\n");
        delete[] fileBuffer;
        return { nullptr, false };
    }

    // ---- map each patch file once ----
    struct PidMap { uint32_t pid; const MappedPkg* mp; };
    std::vector<PidMap> maps;
    maps.reserve(4);

    uint32_t prev = 0xFFFFFFFFu;
    for (int b = firstBlock; b <= lastBlock; ++b) {
        uint32_t pid = Blocks[b].patchID;
        if (pid != prev) { maps.push_back({ pid, nullptr }); prev = pid; }
    }

    for (auto& m : maps) {
        const std::string full = PackageName + "_" + std::to_string(m.pid) + ".pkg";
        m.mp = MapPkgOnce(full);              // uses global mmap cache keyed by full path
        if (!m.mp) {
            std::fprintf(stderr, "Failed to mmap %s\n", full.c_str());
            delete[] fileBuffer;
            return { nullptr, false };
        }
    }

    auto mp_for = [&](uint32_t pid) -> const MappedPkg* {
        for (auto& m : maps) if (m.pid == pid) return m.mp;
        return nullptr;
        };

    // ---- scratch buffers ----
    std::vector<unsigned char> workBuf;
    std::vector<unsigned char> decompBuf(BLOCK_SIZE);
    size_t dstOff = 0;

    auto copy_from_mmap = [&](const MappedPkg* mp, const Block& blk,
        size_t offInBlk, size_t n) {
            const uint8_t* src = mp->ptr(blk.offset + offInBlk);
            std::memcpy(fileBuffer + dstOff, src, n);
            dstOff += n;
        };

    auto process_hot_block = [&](const MappedPkg* mp, const Block& blk,
        size_t takeFrom, size_t n) -> bool {
            if (workBuf.size() < blk.size) workBuf.resize(blk.size);
            const uint8_t* src = mp->ptr(blk.offset);
            std::memcpy(workBuf.data(), src, blk.size);

            unsigned char* io = workBuf.data();
            unsigned char* out = io;
            if (blk.bitFlag & 0x8 and this->hasRedactedKey == false) {
                std::fprintf(stderr, "No Key in place for redacted pkg %s\n",PackageName.c_str());
                return false;
			}
            if (blk.bitFlag & 0x2) {
                unsigned char* dummy = io;
                if (!decryptBlock(blk, io, dummy))
                {
                    return false;
                }
            }
            if (blk.bitFlag & 0x1) {
                unsigned char* de = decompBuf.data();
                decompressBlock(blk, io, de);
                out = de;
            }
            std::memcpy(fileBuffer + dstOff, out + takeFrom, n);
            dstOff += n;
            return true;
        };

    // ---- first block ----
    {
        const Block& blk = Blocks[firstBlock];
        const MappedPkg* mp = mp_for(blk.patchID);
        const size_t take = std::min(
            (size_t)(BLOCK_SIZE - entry.starting_block_offset),
            (size_t)entry.file_size);
        if ((blk.bitFlag & 0x3) == 0) {
            copy_from_mmap(mp, blk, entry.starting_block_offset, take);
        }
        else {
            if (!process_hot_block(mp, blk, entry.starting_block_offset, take)) {
                delete[] fileBuffer;
                return { nullptr, false };
            }
        }
    }

    // ---- middle blocks ----
    int b = firstBlock + 1;
    while (b <= lastBlock - 1) {
        const Block& blk = Blocks[b];
        if ((blk.bitFlag & 0x3) == 0) {
            // coalesce run of plain contiguous blocks
            const uint32_t pid = blk.patchID;
            const MappedPkg* mp = mp_for(pid);
            uint64_t expected = blk.offset;
            size_t runBytes = 0;
            int runStart = b;

            while (b <= lastBlock - 1) {
                const Block& cur = Blocks[b];
                if ((cur.bitFlag & 0x3) != 0 ||
                    cur.patchID != pid ||
                    cur.offset != expected)
                    break;
                runBytes += (size_t)cur.size;       // advance by cur.size
                expected += (uint64_t)cur.size;
                ++b;
            }

            const uint8_t* src = mp->ptr(Blocks[runStart].offset);
            std::memcpy(fileBuffer + dstOff, src, runBytes);
            dstOff += runBytes;
        }
        else {
            const MappedPkg* mp = mp_for(blk.patchID);
            if (!process_hot_block(mp, blk, 0, BLOCK_SIZE)) {
                delete[] fileBuffer;
                return { nullptr, false };
            }
            ++b;
        }
    }

    // ---- last block (if >1 block total) ----
    if (firstBlock != lastBlock) {
        const Block& blk = Blocks[lastBlock];
        const MappedPkg* mp = mp_for(blk.patchID);
        const size_t tail = entry.file_size - dstOff;
        if ((blk.bitFlag & 0x3) == 0) {
            copy_from_mmap(mp, blk, 0, tail);
        }
        else {
            if (!process_hot_block(mp, blk, 0, tail)) {
                delete[] fileBuffer;
                return { nullptr, false };
            }
        }
    }

    ret.success = true;
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