#include <unordered_map>
#include <string>
#include "TigerEngine/package.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <fstream>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include "SpriteBatch.h"
#include "SpriteFont.h"
#include "TigerEngine/tag.h"
#include "TigerEngine/string/string.h"
#include "TigerEngine/Map/static.h"
#include "TigerEngine/helpers.h"
#include "TigerEngine/Technique/technique.h"
#include <DDSTextureLoader.h>
#include <dxgidebug.h> 

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

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
