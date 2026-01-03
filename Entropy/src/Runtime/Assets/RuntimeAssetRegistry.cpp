#include "RuntimeAssetRegistry.h"


void RuntimeAssetRegistry::RegisterBuffer(uint32_t id, BufferPayload payload) {
    std::lock_guard<std::mutex> lk(m_);
    buffers_[id] = std::move(payload);
}
void RuntimeAssetRegistry::RegisterShader(uint32_t id, ShaderPayload payload) {
    std::lock_guard<std::mutex> lk(m_);
    shaders_[id] = std::move(payload);
}
void RuntimeAssetRegistry::RegisterTexture(uint32_t id, Texture2DPayload payload) {
    std::lock_guard<std::mutex> lk(m_);
    textures_[id] = std::move(payload);
}

void RuntimeAssetRegistry::Register3DTexture(uint32_t id, Texture3DPayload payload) {
    std::lock_guard<std::mutex> lk(m_);
    textures3d_[id] = std::move(payload);
}

void RuntimeAssetRegistry::RegisterSampler(uint32_t id, const D3D11_SAMPLER_DESC& desc) {
    std::lock_guard<std::mutex> lk(m_);
    samplers_[id] = desc;
}
void RuntimeAssetRegistry::RegisterCBuffer(uint32_t id, CBufferMeta meta) {
    std::lock_guard<std::mutex> lk(m_);
    cbuffers_[id] = std::move(meta);
}
void RuntimeAssetRegistry::RegisterTechnique(uint32_t techId, TechniqueDesc desc) {
    std::lock_guard<std::mutex> lk(m_);
    techniques_[techId] = std::move(desc);
}

bool RuntimeAssetRegistry::TryGetBuffer(uint32_t id, BufferPayload& out) const noexcept {
    std::scoped_lock lk(m_);
    auto it = buffers_.find(id);
    if (it == buffers_.end()) return false;
    out = it->second;        
    return true;
}


BufferPayload RuntimeAssetRegistry::GetBuffer(uint32_t id) const {
    std::lock_guard<std::mutex> lk(m_);
    return must_find_(buffers_, id);
}
ShaderPayload RuntimeAssetRegistry::GetShader(uint32_t id) const {
    std::lock_guard<std::mutex> lk(m_);
    return must_find_(shaders_, id);
}
Texture2DPayload RuntimeAssetRegistry::GetTexture(uint32_t id) const {
    std::lock_guard<std::mutex> lk(m_);
    return must_find_(textures_, id);
}
Texture3DPayload RuntimeAssetRegistry::Get3DTexture(uint32_t id) const {
    std::lock_guard<std::mutex> lk(m_);
    return must_find_(textures3d_, id);
}

D3D11_SAMPLER_DESC RuntimeAssetRegistry::GetSampler(uint32_t id) const {
    std::lock_guard<std::mutex> lk(m_);
    return must_find_(samplers_, id);
}
CBufferMeta RuntimeAssetRegistry::GetCBuffer(uint32_t id) const {
    std::lock_guard<std::mutex> lk(m_);
    return must_find_(cbuffers_, id);
}
TechniqueDesc RuntimeAssetRegistry::GetTechnique(uint32_t techId) const {
    std::lock_guard<std::mutex> lk(m_);
    return must_find_(techniques_, techId);
}


bool RuntimeAssetRegistry::HasBuffer(uint32_t id) const {
    std::lock_guard<std::mutex> lk(m_);
    return buffers_.count(id) > 0;
}
bool RuntimeAssetRegistry::HasShader(uint32_t id) const {
    std::lock_guard<std::mutex> lk(m_);
    return shaders_.count(id) > 0;
}
bool RuntimeAssetRegistry::HasTexture(uint32_t id) const {
    std::lock_guard<std::mutex> lk(m_);
    return textures_.count(id) > 0;
}
bool RuntimeAssetRegistry::HasSampler(uint32_t id) const {
    std::lock_guard<std::mutex> lk(m_);
    return samplers_.count(id) > 0;
}
bool RuntimeAssetRegistry::HasCBuffer(uint32_t id) const {
    std::lock_guard<std::mutex> lk(m_);
    return cbuffers_.count(id) > 0;
}
bool RuntimeAssetRegistry::HasTechnique(uint32_t id) const {
    std::lock_guard<std::mutex> lk(m_);
    return techniques_.count(id) > 0;
}
