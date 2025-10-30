#include <unordered_map>
#include <cstdint>
#include "TigerEngine/tag.h"


struct namedTagEntry
{
    TagHash tag;
    uint32_t reference;
    std::string name;
};

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

    static std::vector<namedTagEntry>& getNamedTags() {
        static std::vector<namedTagEntry> namedTags;
        return namedTags;
    }

    static std::unordered_map<std::string, TagHash>& getGlobalTechniques() {
        static std::unordered_map<std::string, TagHash> globalTechs;
        return globalTechs;
    }

    static std::vector<std::pair<std::string, TagHash>>& getScopes() {
        static std::vector<std::pair<std::string, TagHash>> scopes;
        return scopes;
    }
};

#pragma once
