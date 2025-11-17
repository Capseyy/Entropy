#include "instance.h"

CB1Payload_override UpdateCB1_Single(
    glm::vec4 model_offset,
    glm::vec4             model_scale,
    float            instance_scale,
    float             texScale,
    float             texOffX, float texOffY,
    const glm::quat& rot,    
    const glm::vec3& pos)
{
    using namespace DirectX;

    CB1Payload_override cb{};

    const XMVECTOR q = XMVectorSet(rot.w, rot.x, rot.y, rot.z);
    const XMMATRIX R = XMMatrixRotationQuaternion(q);
    const XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);

    // Uniform scale goes IN THE MATRIX:
    float s = instance_scale;
    const XMMATRIX S = XMMatrixScaling(s, s, s);

    // Row-vector convention: Scale ? Rotate ? Translate
    const XMMATRIX M = S * R * T;
    XMStoreFloat4x4(&cb.mesh_to_world, M);

    // Keep authored per-mesh scale in cb (unchanged) to avoid double scaling.
    cb.position_scale = XMFLOAT4(model_scale.x, model_scale.y, model_scale.z, model_scale.w);
    cb.position_offset = XMFLOAT4(model_offset.x, model_offset.y, model_offset.z, model_offset.w);

    cb.texcoord0_scale_offset = XMFLOAT4(texScale, texScale, texOffX, texOffY);
    cb.dynamic_sh_ao_values = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

    /*D3D11_MAPPED_SUBRESOURCE m{};
    if (SUCCEEDED(ctx->Map(g_cb1.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        std::memcpy(m.pData, &cb, sizeof(cb));
        ctx->Unmap(g_cb1.Get(), 0);
    }*/
    return cb;
}

