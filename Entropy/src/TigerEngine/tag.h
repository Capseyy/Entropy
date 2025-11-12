#pragma once
#include <filesystem>
#include <unordered_map>
#include "package.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <vector>
#include <span>
#include <limits>
#include <string>
#include <cstdio>
#include <cinttypes>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "TigerEngine/Technique/Tfx/tfx_runtime.h"
#include "TigerEngine/Map/occlusion.h"
#include "Renderer/Graphics/Render/FrustumCulling.h"
#include <glm/gtc/type_ptr.hpp> 
#include "d3d11.h"



struct StringHash {
    uint32_t hash;
    std::string string;
};

class WideHashData {
public:
    uint32_t Unk0{ 0 };
    uint32_t Unk4{ 0 };
    uint64_t Hash64{ 0 };
};

struct RawStringPointer64
{
    int64_t offset;
    std::string name;
};

class Package;

class WideHash {
public:
    WideHash() : size(0), reference(0), success(false), data(nullptr) {}

    explicit WideHash(WideHashData whd)
        : wideHashData(whd), size(0), reference(0), success(false), data(getData()) {
    }
    unsigned char* getData();
    uint32_t size;
    WideHashData wideHashData;
    uint32_t reference;
    bool success;
    unsigned char* data;
    uint32_t tagHash32 = 0;
    void print();
};

template<std::size_t N>
struct SkipTo {};

class TagHash {
public:
    TagHash() : hash(0), data(getData()), success(false), size(0), reference(0) {}

    explicit TagHash(uint32_t h)
        : hash(h), data(getData()), success(false), size(0), reference(0) {
    }
    TagHash(uint32_t h, bool skip)
        : hash(h), data(nullptr), success(false), size(0), reference(0) {
    }
    int getPkgId();
    int getEntryID();
    unsigned char* getData();
    void print_buffer();
    uint32_t size;
    uint32_t hash{};
    uint32_t reference;
    int type;
    int sub_type;
    bool success;
    unsigned char* data;
    void free();
    unsigned char* getDatawithPkg(Package* pkg);
    unsigned char* getDatawithPkg(const Package* pkg);
    std::string GetPackageName();
};

class ResourcePointer {
public:
    uint64_t offset{ 0 };
    uint32_t type{ 0 };
    bool     valid{ false };

    template <typename T>
    inline T Parse(TagHash& tag);   

    template <typename T>
    inline T Parse(const TagHash& owner) const;   // const-correct
                
};

struct RelativePointer64 {
    int64_t offset;
};

// ---- Optional: turn on PFR if available ----
#if !defined(BIN_USE_PFR_OFF)
#include <boost/pfr.hpp>
#define BIN_HAS_PFR 1
#else
#define BIN_HAS_PFR 0
#endif

namespace bin {

    // ---------- Endianness ----------
    enum class Endian { Little, Big };

    namespace detail {
        template<class T>
        constexpr T bswap(T v) {
            if constexpr (sizeof(T) == 1) return v;
            else if constexpr (sizeof(T) == 2) {
                auto x = static_cast<uint16_t>(v);
                x = uint16_t((x >> 8) | (x << 8)); return static_cast<T>(x);
            }
            else if constexpr (sizeof(T) == 4) {
                auto x = static_cast<uint32_t>(v);
                x = (x >> 24) | ((x >> 8) & 0x0000FF00u) | ((x << 8) & 0x00FF0000u) | (x << 24);
                return static_cast<T>(x);
            }
            else if constexpr (sizeof(T) == 8) {
                auto x = static_cast<uint64_t>(v);
                x = (x >> 56) |
                    ((x & 0x00FF000000000000ull) >> 40) |
                    ((x & 0x0000FF0000000000ull) >> 24) |
                    ((x & 0x000000FF00000000ull) >> 8) |
                    ((x & 0x00000000FF000000ull) << 8) |
                    ((x & 0x0000000000FF0000ull) << 24) |
                    ((x & 0x000000000000FF00ull) << 40) |
                    (x << 56);
                return static_cast<T>(x);
            }
            else {
                static_assert(sizeof(T) <= 8, "bswap: unsupported size");
                return v;
            }
        }

        template<class T>
        concept arithmetic_but_not_bool =
            std::is_arithmetic_v<T> && !std::is_same_v<T, bool>;
    }

    // ---------- Reader ----------
    struct Reader {
        std::span<const std::byte> data;
        size_t pos{ 0 };
        Endian src{ Endian::Little };

        size_t remaining() const { return data.size() - pos; }
        void need(size_t n) { if (remaining() < n) throw std::out_of_range("bin::Reader: read past end"); }
        void seek(size_t p) { if (p > data.size()) throw std::out_of_range("bin::Reader: seek past end"); pos = p; }

        template<class T> T read_arith() {
            static_assert(std::is_arithmetic_v<T>, "arith only");
            need(sizeof(T));
            T v{};
            std::memcpy(&v, data.data() + pos, sizeof(T));
            pos += sizeof(T);
            if constexpr (sizeof(T) > 1) if (src == Endian::Big) v = detail::bswap(v);
            return v;
        }

        std::span<const std::byte> read_bytes(size_t n) {
            need(n);
            auto s = data.subspan(pos, n);
            pos += n;
            return s;
        }
    };

    // ---------- Forward decl ----------
    template<class T> void read_into(Reader& r, T& x);

    // ---------- Primitives ----------
    inline void read_into(Reader& r, bool& b) {
        b = r.read_arith<uint8_t>() != 0;
    }
    template<detail::arithmetic_but_not_bool T>
    inline void read_into(Reader& r, T& v) {
        v = r.read_arith<T>();
    }
    inline void read_into(Reader& r, glm::quat& q) { 
        q.w = r.read_arith<float>(); q.x = r.read_arith<float>(); q.y = r.read_arith<float>(); q.z = r.read_arith<float>(); 
    }

    void read_into(Reader& r, StringHash& q);

    inline void read_into(Reader& r, RawStringPointer64& q) {
        int base = r.pos;
        q.offset = r.read_arith<uint64_t>();
        r.seek(q.offset + base);
        std::string raw_name;
        for (;;) {
            auto byte = r.read_arith<char>();
            if (byte == '\0') break;
            raw_name.push_back(byte);
        }
        q.name = raw_name;
        r.seek(base + 0x8);
    }

    template<std::size_t N>
    inline void read_into(Reader& r, SkipTo<N>&) {
        //r.need(N);
        r.seek(N);   // advance by N bytes, no alloc, no copy
    }
   
    inline void read_into(Reader& r, Aabb& q) {
        DirectX::XMFLOAT3 vec;
        vec.x = r.read_arith<float_t>();
        vec.y = r.read_arith<float_t>();
        vec.z = r.read_arith<float_t>();
        r.read_arith<float_t>();
        q.minv = vec;
        DirectX::XMFLOAT3 vec1;
        vec1.x = r.read_arith<float_t>();
        vec1.y = r.read_arith<float_t>();
        vec1.z = r.read_arith<float_t>();
        r.read_arith<float_t>();
        q.maxv = vec1;
    }

    inline void read_into(Reader& r, glm::vec4& v) {
        v.x = r.read_arith<float>();
        v.y = r.read_arith<float>();
        v.z = r.read_arith<float>();
        v.w = r.read_arith<float>();
    }

    inline void read_into(Reader& r, glm::mat4& m) {
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                m[col][row] = r.read_arith<float>();
    }

    inline void read_into(Reader& r, glm::vec3& q) {
        q.x = r.read_arith<float>(); q.y = r.read_arith<float>(); q.z = r.read_arith<float>();
    }
    inline void read_into(Reader& r, glm::vec2& q) {
        q.x = r.read_arith<float>(); q.y = r.read_arith<float>();
    }
    inline void read_into(Reader& r, Vec4& q) {
        q.x = r.read_arith<float>(); q.y = r.read_arith<float>(); q.z = r.read_arith<float>(); q.w = r.read_arith<float>();
    }

    inline void read_into(Reader& r, TagHash& t) {
        t.hash = r.read_arith<uint32_t>();
        t.getData();
    }
    inline void read_into(Reader& r, WideHash& t) {
        auto whd = WideHashData{};
        whd.Unk0 = r.read_arith<uint32_t>();
        whd.Unk4 = r.read_arith<uint32_t>();
        whd.Hash64 = r.read_arith<uint64_t>();
        t.wideHashData = whd;
        t.getData();
    }

    inline void read_into(Reader& r, RelativePointer64& t) {
        auto startPos = r.pos;
        t.offset = r.read_arith<int64_t>() + startPos;
    }
    inline void read_into(Reader& r, ResourcePointer& t) {
        auto startPos = r.pos;
        auto offset = r.read_arith<int64_t>();
        r.seek(startPos + offset - 4);
        t.type = r.read_arith<int32_t>();
        t.offset = offset + startPos;
        r.seek(startPos+8);
    }
    // ---------- std::string: [u32 len][bytes] (no null) ----------
    inline void read_into(Reader& r, std::string& s) {
        uint32_t n = r.read_arith<uint32_t>();
        auto bytes = r.read_bytes(n);
        s.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    template <class T, std::size_t N>
    inline void read_into(Reader& r, std::array<T, N>& arr) {
        // Optional sanity guard when we can compute the total footprint
        if constexpr (std::is_trivially_default_constructible_v<T> &&
            std::is_trivially_destructible_v<T>) {
            const std::size_t need = N * sizeof(T);
            if (need > r.remaining())
                throw std::out_of_range("stf::array: elements exceed remaining bytes");
        }

        // Fast path: treat any 32-bit unsigned integral as scalar reads
        constexpr bool is_u32_like =
            std::is_integral_v<std::remove_cv_t<T>> &&
            std::is_unsigned_v<std::remove_cv_t<T>> &&
            sizeof(std::remove_cv_t<T>) == 4;

        for (std::size_t i = 0; i < N; ++i) {
            if constexpr (is_u32_like) {
                arr[i] = r.read_arith<T>();
            }
            else {
                // General path: delegate to element reader (structs, other scalars)
                T elem{};
                read_into(r, elem);      // rely on your existing overloads for T
                arr[i] = std::move(elem);
            }
        }
    }
    // ---------- std::vector<T>: [u32 count][T…] ----------
    template<class T>
    inline void read_into(Reader& r, std::vector<T>& vec) {
        // Header: [u32 count][u64 relOffsetFromHere]
        const uint32_t count = r.read_arith<uint64_t>();
        const uint64_t offset = r.read_arith<uint64_t>();
        // Base = position right after the header (current r.pos)
        const size_t base = r.pos;
        if (count != 0) {
            //std::cout << offset;
            //std::cout << "\n";
            // Bounds: base + offset must be within the buffer
            if (offset > static_cast<uint64_t>(r.data.size()))
                throw std::out_of_range("vector: offset too large");

            const size_t target = base + static_cast<size_t>(offset + 8);
            if (target > r.data.size())
                throw std::out_of_range("vector: target beyond buffer");

            // Jump to the elements
            r.seek(target);

            // Optional sanity guard for fixed-size elements (like SStringPart = 32 bytes)
            if constexpr (std::is_trivially_default_constructible_v<T> && std::is_trivially_destructible_v<T>) {
                const size_t need = static_cast<size_t>(count) * sizeof(T);
                if (need > r.remaining())
                    throw std::out_of_range("vector: elements exceed remaining bytes");
            }

            // Read elements
            vec.clear();
            vec.reserve(static_cast<size_t>(count));
            for (size_t i = 0; i < static_cast<size_t>(count); ++i) {
                T elem{};
                read_into(r, elem);
                vec.emplace_back(std::move(elem));

            }

        }

        // Restore so parsing can continue after the header
        r.seek(base);
    }

    // ---------- Aggregates (structs) ----------
    // Path A: Boost.PFR — zero boilerplate
    template<class T>
    inline void read_aggregate(Reader& r, T& x) {
        boost::pfr::for_each_field(x, [&](auto& field) {
            read_into(r, field);
            });
    }

    // Dispatch for user-defined types (neither arithmetic nor std::vector/string)
    template<class T>
    inline void read_into(Reader& r, T& x) {
        if constexpr (std::is_arithmetic_v<T> || std::is_same_v<T, bool>) {
            // already handled by the primitive overloads above
            if constexpr (!std::is_same_v<T, bool>) {
                x = r.read_arith<T>();
            }
            else {
                x = r.read_arith<uint8_t>() != 0;
            }
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            read_into(r, x);
        }
        else if constexpr (requires { typename T::value_type; x.size(); }) {
            // this branch is for containers but std::vector has its own overload already,
            // so we intentionally don't make this generic to avoid surprises.
            static_assert(!sizeof(T*), "Unsupported container type. Provide a read_into overload.");
        }
        else {
            read_aggregate(r, x); // walk fields via PFR or fallback
        }
    }

    // ---------- Top-level convenience ----------
    template<class T>
    inline T parse(const unsigned char* blob, size_t size, Endian src = Endian::Little) {
        Reader rr{ std::span<const std::byte>{reinterpret_cast<const std::byte*>(blob), size}, 0, src };
        T out{};                       // keep this
        rr.src = src;
        read_into(rr, out);
        return out;
    }

} // namespace bin

// ---------- Fallback macro (only if you DISABLE Boost.PFR) ----------
// Usage inside a struct body: BIN_REFLECTABLE(MyType, field1, field2, field3)
#if !BIN_HAS_PFR
#include <tuple>
#define BIN_REFLECTABLE(Type, ...)                                           \
    friend auto bin_reflect(Type& self) {                                      \
        return std::tie(__VA_ARGS__);                                          \
    }
#endif

template <typename T>
inline T ResourcePointer::Parse(TagHash& tag) {
    if (!tag.data) {
        throw std::invalid_argument("ResourcePointer::Parse: TagHash.data is null");
    }
    if (offset > static_cast<uint64_t>(tag.size)) {
        throw std::out_of_range("ResourcePointer::Parse: offset beyond tag buffer");
    }
    const unsigned char* ptr = tag.data + static_cast<size_t>(offset);
    const std::size_t remaining = static_cast<std::size_t>(tag.size - offset);
    return bin::parse<T>(ptr, remaining);
}

template <typename T>
inline T ResourcePointer::Parse(const TagHash& tag) const{
    if (!tag.data) {
        throw std::invalid_argument("ResourcePointer::Parse: TagHash.data is null");
    }
    if (offset > static_cast<uint64_t>(tag.size)) {
        throw std::out_of_range("ResourcePointer::Parse: offset beyond tag buffer");
    }
    const unsigned char* ptr = tag.data + static_cast<size_t>(offset);
    const std::size_t remaining = static_cast<std::size_t>(tag.size - offset);
    return bin::parse<T>(ptr, remaining);
}
//template <typename T>
//inline T ResourcePointer::Parse(TagHash tag) {
//    return Parse<T>(static_cast<const TagHash&>(tag));
//}
