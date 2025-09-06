#include <unordered_map>
#include <cstdint>
#include "TigerEngine/tag.h"

class GlobalData {
public:
    // Provides access to the single shared unordered_map
    static std::unordered_map<int, Package>& getMap() {
        static std::unordered_map<int, Package> PackageCache;
        return PackageCache;
    }

    static std::unordered_map<uint64_t, TagHash>& getH64() {
        static std::unordered_map<uint64_t, TagHash> h64_cache;
        return h64_cache;
    }
};

#pragma once
