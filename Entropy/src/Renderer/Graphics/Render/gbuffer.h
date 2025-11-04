// GBuffer.h

#include <wrl/client.h>
#include <d3d11.h>
#include <string>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <atomic>
#include "d3dcompiler.h"

inline HRESULT SetDebugName(ID3D11DeviceChild* obj, const char* name) {
#if defined(_DEBUG)
    if (!obj || !name) return S_OK;
    return obj->SetPrivateData(WKPDID_D3DDebugObjectName, UINT(strlen(name)), name);
#else
    (void)obj; (void)name; return S_OK;
#endif
}

static HRESULT CompileVSFromMemory(
    ID3D11Device* device,
    const char* source,
    const char* entryPoint,
    const char* target,
    ID3D11VertexShader** outVS,
    ID3DBlob** outBytecode)   // keep bytecode so you can make the input layout
{
    UINT flags = 0;

    flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;

    Microsoft::WRL::ComPtr<ID3DBlob> bytecode, errors;
    HRESULT hr = D3DCompile(
        source, strlen(source),
        /*sourceName*/ "SkinnedHackVS.hlsl",
        /*defines*/ nullptr,
        /*include*/ nullptr,
        entryPoint, target, flags, 0,
        bytecode.GetAddressOf(), errors.GetAddressOf());

    if (FAILED(hr)) {
        if (errors) OutputDebugStringA((const char*)errors->GetBufferPointer());
        return hr;
    }

    hr = device->CreateVertexShader(
        bytecode->GetBufferPointer(),
        bytecode->GetBufferSize(),
        nullptr,
        outVS);
    if (FAILED(hr)) return hr;

    if (outBytecode) {
        *outBytecode = bytecode.Detach();
    }
    return S_OK;
}

struct RenderTarget {
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          tex;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   rtv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    DXGI_FORMAT baseFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT rtvFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;
    std::string name;

    RenderTarget() = default;
    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    RenderTarget(RenderTarget&&) noexcept = default;
    RenderTarget& operator=(RenderTarget&&) noexcept = default;

    void Reset() {
        tex.Reset(); rtv.Reset(); srv.Reset();
        baseFormat = rtvFormat = srvFormat = DXGI_FORMAT_UNKNOWN;
        name.clear();
    }
};

struct CpuStagingBuffer {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
    std::string name;

    CpuStagingBuffer() = default;
    CpuStagingBuffer(const CpuStagingBuffer&) = delete;
    CpuStagingBuffer& operator=(const CpuStagingBuffer&) = delete;
    CpuStagingBuffer(CpuStagingBuffer&&) noexcept = default;
    CpuStagingBuffer& operator=(CpuStagingBuffer&&) noexcept = default;
};

struct DepthState {
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          tex, texCopy;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   dsv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv, texCopySRV;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  dsWrite, dsReadOnly;

    DepthState() = default;
    DepthState(const DepthState&) = delete;
    DepthState& operator=(const DepthState&) = delete;
    DepthState(DepthState&&) noexcept = default;
    DepthState& operator=(DepthState&&) noexcept = default;

    void Reset() {
        tex.Reset(); texCopy.Reset(); dsv.Reset(); srv.Reset();
        texCopySRV.Reset(); dsWrite.Reset(); dsReadOnly.Reset();
    }
};

// -------- factories: out-parameter, single definition in .cpp --------
bool CreateColorRT(ID3D11Device* dev, UINT w, UINT h,
    DXGI_FORMAT texTypeless, DXGI_FORMAT rtvFmt, DXGI_FORMAT srvFmt,
    const char* debugName, RenderTarget& out);


bool CreateStaging(ID3D11Device* dev, UINT w, UINT h,
    DXGI_FORMAT fmt, const char* name, CpuStagingBuffer& out);

bool CreateDepthState(ID3D11Device* dev, UINT w, UINT h,
    const char* name, DepthState& out);

struct GBuffer {
    mutable Microsoft::WRL::ComPtr<ID3D11Texture2D> lightDiffuseReadback1x1; // 1×1 staging

    bool ReadLightDiffusePixel(ID3D11DeviceContext* ctx, UINT x, UINT y,
        DirectX::XMFLOAT3& outRGB) const;

    RenderTarget rt0, rt1, rt1_read, rt2, rt3;
    RenderTarget light_diffuse, light_specular, light_ibl_specular;
    RenderTarget shading_result, shading_result_read;
    DepthState   depth;
    CpuStagingBuffer depth_staging;
    RenderTarget ssao_intermediate, atmos_ss_far_lookup, atmos_ss_near_lookup, depth_angle_density_lookup;
    RenderTarget postprocess_ping, postprocess_pong;
    bool pingIsA = true;
    UINT w = 0, h = 0;

    void Create(ID3D11Device* dev, UINT W, UINT H);
    void CreateFromContext(ID3D11DeviceContext* ctx, UINT W, UINT H);
    void Resize(ID3D11Device* dev, UINT W, UINT H);
    void BindGBufferForWriting(ID3D11DeviceContext* ctx) const;
    void UnbindMRTs(ID3D11DeviceContext* ctx) const;

public:
    // Ping–pong helpers
    void GetPostprocessRT(RenderTarget*& src, RenderTarget*& dst, bool swapAfterUse);
    RenderTarget* GetPostprocessOutput();

    // CPU depth readback helpers
    void CopyDepthToStaging(ID3D11DeviceContext* ctx) const;
    bool ReadDepthPixel(ID3D11DeviceContext* ctx, UINT x, UINT y, float& outDepth) const;

private:
    std::atomic<bool> post_is_ping{ true };
};
