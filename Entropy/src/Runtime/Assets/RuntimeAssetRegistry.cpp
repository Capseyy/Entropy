#include "RuntimeAssetRegistry.h"

// -------- Register --------
void RuntimeAssetRegistry::RegisterBuffer(uint32_t id, BufferPayload payload) {
    std::lock_guard<std::mutex> lk(m_);
    buffers_[id] = std::move(payload);
}
void RuntimeAssetRegistry::RegisterShader(uint32_t id, ShaderPayload payload) {
    std::lock_guard<std::mutex> lk(m_);
    shaders_[id] = std::move(payload);
}
void RuntimeAssetRegistry::RegisterTexture(uint32_t id, TexturePayload payload) {
    std::lock_guard<std::mutex> lk(m_);
    textures_[id] = std::move(payload);
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

// -------- Get --------
BufferPayload RuntimeAssetRegistry::GetBuffer(uint32_t id) const {
    std::lock_guard<std::mutex> lk(m_);
    return must_find_(buffers_, id);
}
ShaderPayload RuntimeAssetRegistry::GetShader(uint32_t id) const {
    std::lock_guard<std::mutex> lk(m_);
    return must_find_(shaders_, id);
}
TexturePayload RuntimeAssetRegistry::GetTexture(uint32_t id) const {
    std::lock_guard<std::mutex> lk(m_);
    return must_find_(textures_, id);
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

// -------- Has? --------
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
