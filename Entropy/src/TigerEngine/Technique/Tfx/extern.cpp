#include "tfx.h"
#include <cstring>

static inline const ExternStorage::Blob* find_blob(
    const ExternStorage& s, TfxExtern id)
{
    auto it = s.blobs.find(id);
    return (it == s.blobs.end()) ? nullptr : &it->second;
}

float ExternStorage::getFloat(TfxExtern id, size_t byteOffset) const {
    const auto* b = find_blob(*this, id);
    if (!b || !b->ptr || byteOffset + sizeof(float) > b->size) return 0.0f;
    float out;
    std::memcpy(&out, b->ptr + byteOffset, sizeof(float));
    return out;
}

Vec4 ExternStorage::getVec4(TfxExtern id, size_t byteOffset) const {
    const auto* b = find_blob(*this, id);
    if (!b || !b->ptr || byteOffset + sizeof(Vec4) > b->size) return Vec4{ 0,0,0,0 };
    Vec4 out;
    std::memcpy(&out, b->ptr + byteOffset, sizeof(Vec4));
    return out;
}

Mat4 ExternStorage::getMat4(TfxExtern id, size_t byteOffset) const {
    const auto* b = find_blob(*this, id);
    if (!b || !b->ptr || byteOffset + sizeof(Mat4) > b->size) {
        // identity fallback
        return Mat4{
            Vec4{1,0,0,0},
            Vec4{0,1,0,0},
            Vec4{0,0,1,0},
            Vec4{0,0,0,1}
        };
    }
    Mat4 out;
    std::memcpy(&out, b->ptr + byteOffset, sizeof(Mat4));
    return out;
}
