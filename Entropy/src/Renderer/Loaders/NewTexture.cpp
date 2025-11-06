// TextureLoader.cpp
#include "NewTexture.h"
#include "TigerEngine/Technique/texture.h"
using Microsoft::WRL::ComPtr;

// ---- 1:1 “load_data(hash, load_full_mip)” ----
static std::optional<std::pair<STextureHeader, std::vector<uint8_t>>>
LoadTextureData(TagHash hash, bool load_full_mip)
{
    // In Rust they use package_manager() + get_entry(hash).reference.
    // Here we assume TagHash(hash) is the header blob itself; its .reference
    // refers to the other piece (small/large buffer). Adjust to your TagHash API if needed.
    if (!hash.data || !hash.size) return std::nullopt;
    const auto header = bin::parse<STextureHeader>(hash.data, hash.size, bin::Endian::Little);

    std::vector<uint8_t> texture_data;
    if (header.large_buffer.data && header.large_buffer.size) {
        // large_buffer exists ? read that first
        const auto* p = static_cast<const uint8_t*>(header.large_buffer.data);
        texture_data.insert(texture_data.end(), p, p + header.large_buffer.size);
    }
    else {
        // fallback to the header reference bytes
        TagHash ref(hash.reference);
        if (!ref.data || !ref.size) return std::nullopt;
        const auto* p = static_cast<const uint8_t*>(ref.data);
        texture_data.insert(texture_data.end(), p, p + ref.size);
    }

    if (load_full_mip && header.large_buffer.data && header.large_buffer.size) {
        // append the “other half” like Rust
        TagHash ref(hash.reference);
        if (ref.data && ref.size) {
            const auto* p = static_cast<const uint8_t*>(ref.data);
            texture_data.insert(texture_data.end(), p, p + ref.size);
        }
    }

    return std::make_pair(header, std::move(texture_data));
}

// ---- 1:1 loader (Texture::load) ----
std::optional<LoadedTexture> LoadTexture(ID3D11Device* device, TagHash hash)
{
    auto ld = LoadTextureData(hash, /*load_full_mip=*/true);
    if (!ld) return std::nullopt;
    const STextureHeader& tex = ld->first;
    const std::vector<uint8_t>& bytes = ld->second;

    const DXGI_FORMAT fmt = static_cast<DXGI_FORMAT>(tex.dxgiFormat);

    if (tex.depth > 1) {
        // -------- Texture3D path (mips = 1) ----------
        auto [rowPitch, slicePitch] = calculate_pitch(fmt, tex.width, tex.height);
        D3D11_SUBRESOURCE_DATA s{};
        s.pSysMem = bytes.data();
        s.SysMemPitch = static_cast<UINT>(rowPitch);
        s.SysMemSlicePitch = static_cast<UINT>(slicePitch);

        ComPtr<ID3D11Texture3D> t3d;
        D3D11_TEXTURE3D_DESC d{};
        d.Width = tex.width;
        d.Height = tex.height;
        d.Depth = tex.depth;
        d.MipLevels = 1;
        d.Format = fmt;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device->CreateTexture3D(&d, &s, &t3d))) return std::nullopt;

        ComPtr<ID3D11ShaderResourceView> srv;
        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = fmt;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
        sd.Texture3D.MostDetailedMip = 0;
        sd.Texture3D.MipLevels = 1;
        if (FAILED(device->CreateShaderResourceView(t3d.Get(), &sd, &srv))) return std::nullopt;

        LoadedTexture out{};
        out.view = srv;
        out.handle.kind = TextureHandleKind::Tex3D;
        out.handle.tex3D = t3d;
        out.format = fmt;
        return out;
    }
    else if (tex.arraySize > 1) {
        // -------- TextureCube path ----------
        // Build subresources per (mip, arraySlice)
        const UINT mipCount = tex.mipCount;
        std::vector<D3D11_SUBRESOURCE_DATA> initial;
        initial.resize(size_t(mipCount) * size_t(tex.arraySize));

        size_t offset = 0;
        for (UINT mip = 0; mip < mipCount; ++mip) {
            const UINT w = std::max<UINT>(1, tex.width >> mip);
            const UINT h = std::max<UINT>(1, tex.height >> mip);
            auto [rowPitch, slicePitch] = calculate_pitch(fmt, w, h);
            for (UINT e = 0; e < tex.arraySize; ++e) {
                const UINT sub = calc_dx_subresource(mip, e, mipCount);
                initial[sub].pSysMem = bytes.data() + offset;
                initial[sub].SysMemPitch = static_cast<UINT>(rowPitch);
                initial[sub].SysMemSlicePitch = 0;
                offset += slicePitch;
            }
        }

        ComPtr<ID3D11Texture2D> t2d;
        D3D11_TEXTURE2D_DESC d{};
        d.Width = tex.width;
        d.Height = tex.height;
        d.MipLevels = mipCount;
        d.ArraySize = tex.arraySize;
        d.Format = fmt;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        d.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
        if (FAILED(device->CreateTexture2D(&d, initial.data(), &t2d))) return std::nullopt;

        ComPtr<ID3D11ShaderResourceView> srv;
        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = fmt;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        sd.TextureCube.MostDetailedMip = 0;
        sd.TextureCube.MipLevels = mipCount;
        if (FAILED(device->CreateShaderResourceView(t2d.Get(), &sd, &srv))) return std::nullopt;

        LoadedTexture out{};
        out.view = srv;
        out.handle.kind = TextureHandleKind::TexCube;
        out.handle.tex2D = t2d;
        out.format = fmt;
        return out;
    }
    else {
        // -------- Texture2D path ----------
        // Rust logic: if no large_buffer, trust only mip0 (mipcount_fixed = 1)
        UINT mipcount_fixed = (tex.large_buffer.data && tex.large_buffer.size) ? tex.mipCount : 1;

        std::vector<D3D11_SUBRESOURCE_DATA> initial;
        size_t offset = 0;
        for (UINT i = 0; i < mipcount_fixed; ++i) {
            const UINT w = std::max<UINT>(1, tex.width >> i);
            const UINT h = std::max<UINT>(1, tex.height >> i);
            auto [rowPitch, slicePitch] = calculate_pitch(fmt, w, h);
            if (rowPitch == 0) { mipcount_fixed = i; break; }

            D3D11_SUBRESOURCE_DATA s{};
            s.pSysMem = bytes.data() + offset;
            s.SysMemPitch = static_cast<UINT>(rowPitch);
            s.SysMemSlicePitch = 0;
            initial.push_back(s);
            offset += slicePitch;
        }

        UINT verylowres_mip = 0;
        if (LOW_RES.load(std::memory_order_relaxed)) {
            std::vector<D3D11_SUBRESOURCE_DATA> filtered;
            for (UINT i = 0; i < mipcount_fixed; ++i) {
                const UINT w = std::max<UINT>(1, tex.width >> i);
                const UINT h = std::max<UINT>(1, tex.height >> i);
                if (w <= 4 || h <= 4) {
                    if (verylowres_mip == 0) verylowres_mip = i;
                    filtered.push_back(initial[i]);
                }
            }
            if (!filtered.empty()) initial.swap(filtered);
        }

        if (mipcount_fixed < 1) {
            // mirrors Rust error! log
            return std::nullopt;
        }

        ComPtr<ID3D11Texture2D> t2d;
        D3D11_TEXTURE2D_DESC d{};
        d.Width = std::max<UINT>(1, tex.width >> verylowres_mip);
        d.Height = std::max<UINT>(1, tex.height >> verylowres_mip);
        d.MipLevels = static_cast<UINT>(initial.size());
        d.ArraySize = 1;
        d.Format = fmt;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device->CreateTexture2D(&d, initial.empty() ? nullptr : initial.data(), &t2d)))
            return std::nullopt;

        ComPtr<ID3D11ShaderResourceView> srv;
        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = fmt;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MostDetailedMip = 0;
        sd.Texture2D.MipLevels = static_cast<UINT>(initial.size());
        if (FAILED(device->CreateShaderResourceView(t2d.Get(), &sd, &srv))) return std::nullopt;

        LoadedTexture out{};
        out.view = srv;
        out.handle.kind = TextureHandleKind::Tex2D;
        out.handle.tex2D = t2d;
        out.format = fmt;
        return out;
    }
}

// ---- raw helpers (load_2d_raw / load_3d_raw) ----
std::optional<LoadedTexture> Load2DRaw(
    ID3D11Device* device, UINT width, UINT height,
    const uint8_t* data, DXGI_FORMAT fmt, const char* name = nullptr)
{
    auto [rowPitch, slicePitch] = calculate_pitch(fmt, width, height);
    D3D11_SUBRESOURCE_DATA s{};
    s.pSysMem = data;
    s.SysMemPitch = static_cast<UINT>(rowPitch);
    s.SysMemSlicePitch = 0;

    ComPtr<ID3D11Texture2D> t2d;
    D3D11_TEXTURE2D_DESC d{};
    d.Width = width; d.Height = height;
    d.MipLevels = 1; d.ArraySize = 1;
    d.Format = fmt;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device->CreateTexture2D(&d, &s, &t2d))) return std::nullopt;

    ComPtr<ID3D11ShaderResourceView> srv;
    D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = fmt;
    sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sd.Texture2D.MostDetailedMip = 0;
    sd.Texture2D.MipLevels = 1;
    if (FAILED(device->CreateShaderResourceView(t2d.Get(), &sd, &srv))) return std::nullopt;

    LoadedTexture out{};
    out.view = srv;
    out.handle.kind = TextureHandleKind::Tex2D;
    out.handle.tex2D = t2d;
    out.format = fmt;
    return out;
}

std::optional<LoadedTexture> Load3DRaw(
    ID3D11Device* device, UINT width, UINT height, UINT depth,
    const uint8_t* data, DXGI_FORMAT fmt, const char* name = nullptr)
{
    auto [rowPitch, slicePitch] = calculate_pitch(fmt, width, height);
    D3D11_SUBRESOURCE_DATA s{};
    s.pSysMem = data;
    s.SysMemPitch = static_cast<UINT>(rowPitch);
    s.SysMemSlicePitch = static_cast<UINT>(slicePitch);

    ComPtr<ID3D11Texture3D> t3d;
    D3D11_TEXTURE3D_DESC d{};
    d.Width = width; d.Height = height; d.Depth = depth;
    d.MipLevels = 1;
    d.Format = fmt;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device->CreateTexture3D(&d, &s, &t3d))) return std::nullopt;

    ComPtr<ID3D11ShaderResourceView> srv;
    D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = fmt;
    sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    sd.Texture3D.MostDetailedMip = 0;
    sd.Texture3D.MipLevels = 1;
    if (FAILED(device->CreateShaderResourceView(t3d.Get(), &sd, &srv))) return std::nullopt;

    LoadedTexture out{};
    out.view = srv;
    out.handle.kind = TextureHandleKind::Tex3D;
    out.handle.tex3D = t3d;
    out.format = fmt;
    return out;
}
