#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <vector>
#include <array>
#include <unordered_map>
#include <string>
#undef min
#undef max


struct Vec4 {
    float x, y, z, w;

    constexpr Vec4() : x(0), y(0), z(0), w(0) {}
    constexpr Vec4(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}

    static constexpr Vec4 zero() { return Vec4(0, 0, 0, 0); }
    static constexpr Vec4 one() { return Vec4(1, 1, 1, 1); }
    static constexpr Vec4 X() { return Vec4(1, 0, 0, 0); }
    static constexpr Vec4 Y() { return Vec4(0, 1, 0, 0); }
    static constexpr Vec4 Z() { return Vec4(0, 0, 1, 0); }
    static constexpr Vec4 W() { return Vec4(0, 0, 0, 1); }
    static constexpr Vec4 splat(float v) { return Vec4(v, v, v, v); }

    
    Vec4 operator+(const Vec4& b) const { return { x + b.x, y + b.y, z + b.z, w + b.w }; }
    Vec4 operator-(const Vec4& b) const { return { x - b.x, y - b.y, z - b.z, w - b.w }; }
    Vec4 operator*(const Vec4& b) const { return { x * b.x, y * b.y, z * b.z, w * b.w }; }
    Vec4 operator/(const Vec4& b) const { return { x / b.x, y / b.y, z / b.z, w / b.w }; }

    
    Vec4 operator*(float s) const { return { x * s, y * s, z * s, w * s }; }
    Vec4 operator/(float s) const { return { x / s, y / s, z / s, w / s }; }
};
inline Vec4 operator*(float s, const Vec4& v) { return v * s; }

inline float  dot(const Vec4& a, const Vec4& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
inline Vec4   minv(const Vec4& a, const Vec4& b) { return { std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z),std::min(a.w,b.w) }; }
inline Vec4   maxv(const Vec4& a, const Vec4& b) { return { std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z),std::max(a.w,b.w) }; }
inline Vec4   clampv(const Vec4& v, const Vec4& lo, const Vec4& hi) { return maxv(lo, minv(v, hi)); }
inline Vec4   abs(const Vec4& v) { return { std::fabs(v.x),std::fabs(v.y),std::fabs(v.z),std::fabs(v.w) }; }
inline Vec4   sign(const Vec4& v) { auto s = [](float f) {return (f > 0) - (f < 0); }; return { float(s(v.x)),float(s(v.y)),float(s(v.z)),float(s(v.w)) }; }
inline Vec4   floor(const Vec4& v) { return { std::floor(v.x),std::floor(v.y),std::floor(v.z),std::floor(v.w) }; }
inline Vec4   ceil(const Vec4& v) { return { std::ceil(v.x),std::ceil(v.y),std::ceil(v.z),std::ceil(v.w) }; }
inline Vec4   round(const Vec4& v) { return { std::round(v.x),std::round(v.y),std::round(v.z),std::round(v.w) }; }
inline Vec4   frac(const Vec4& v) { auto f = [](float a) {return a - std::floor(a); }; return { f(v.x),f(v.y),f(v.z),f(v.w) }; }
inline Vec4   saturate(const Vec4& v) { return clampv(v, Vec4::zero(), Vec4::one()); }

struct Mat4 {
    
    Vec4 x_axis, y_axis, z_axis, w_axis;
    static Mat4 identity() { return { Vec4::X(),Vec4::Y(),Vec4::Z(),Vec4::W() }; }
    Vec4 mul_vec4(const Vec4& v) const {
        
        return {
            v.x * x_axis.x + v.y * y_axis.x + v.z * z_axis.x + v.w * w_axis.x,
            v.x * x_axis.y + v.y * y_axis.y + v.z * z_axis.y + v.w * w_axis.y,
            v.x * x_axis.z + v.y * y_axis.z + v.z * z_axis.z + v.w * w_axis.z,
            v.x * x_axis.w + v.y * y_axis.w + v.z * z_axis.w + v.w * w_axis.w,
        };
    }
};


struct CBufferRegistry {
    
    std::unordered_map<int, std::vector<Vec4>> slots;

    std::vector<Vec4>& get(int slot, size_t minSize = 0) {
        auto& v = slots[slot];
        if (v.size() < minSize) v.resize(minSize, Vec4::zero());
        return v;
    }
};


enum class TfxShaderStage : uint8_t { Vertex = 0, Pixel = 1, Geometry = 2, Compute = 3, Hull = 4, Domain = 5, Unknown = 7 };
inline uint8_t        DecodeSlotFromPacked(uint8_t packed) { return packed & 0x1F; }


inline std::string decode_permute_param(uint8_t fields) {
    auto ch = [&](uint8_t v)->char { switch (v & 3) { case 0:return 'x'; case 1:return 'y'; case 2:return 'z'; default:return 'w'; } };
                                                            std::string s = ".";
                                                            s.push_back(ch(fields >> 6));
                                                            s.push_back(ch(fields >> 4));
                                                            s.push_back(ch(fields >> 2));
                                                            s.push_back(ch(fields >> 0));
                                                            return s;
}
