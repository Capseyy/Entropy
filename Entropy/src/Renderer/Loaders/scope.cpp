#include "Scope.h"
#include "TigerEngine/Technique/Tfx/tfx_program.h"

ComPtr<ID3D11Buffer> TigerScope::GetPSCBuffer() {
	return this->pscbuffer;
}
ComPtr<ID3D11Buffer> TigerScope::GetVSCBuffer() {
	return this->vscbuffer;
}

inline FrameAuxCB MakeFrameAuxFromRustDefaults() { return FrameAuxCB{}; }

// Match Rust #[repr(C)] Vec4[37] layout exactly.
// sizeof(Vec4) must be 16; sizeof(ScopeTransparentAdvancedCB) must be 592.
struct ScopeTransparentAdvancedCB {
    Vec4 unk0, unk1, unk2, unk3, unk4, unk5, unk6, unk7, unk8, unk9,
        unk10, unk11, unk12, unk13, unk14, unk15, unk16, unk17, unk18, unk19,
        unk20, unk21, unk22, unk23, unk24, unk25, unk26, unk27, unk28, unk29,
        unk30, unk31, unk32, unk33, unk34, unk35, unk36;
};

static_assert(sizeof(Vec4) == 16, "Vec4 must be 16 bytes");
static_assert(sizeof(ScopeTransparentAdvancedCB) == 37 * 16, "CB size must be 592 bytes");

inline ScopeTransparentAdvancedCB MakeScopeTransparentAdvancedFromRustDefaults()
{
    ScopeTransparentAdvancedCB cb{};
    cb.unk0 = { 0.0009849314f, 0.0019836868f, 0.0007783567f, 0.0015586712f };
    cb.unk1 = { 0.00098604f,   0.002085914f,  0.0009838239f, 0.0018864698f };
    cb.unk2 = { 0.0011860824f, 0.0024346288f, 0.0009468408f, 0.001850187f };
    cb.unk3 = { 0.7903466f,    0.7319064f,    0.56213695f,   0.0f };
    cb.unk4 = { 0.0f,          1.0f,          0.109375f,     0.046875f };
    cb.unk5 = { 0.0f,          0.0f,          0.0f,          0.00086945295f };
    cb.unk6 = { 0.55f,         0.41091052f,   0.22670946f,   0.50381273f };
    cb.unk7 = { 1.0f,          1.0f,          1.0f,          0.9997778f };
    cb.unk8 = { 132.92885f,    66.40444f,     56.853416f,    0.0f };
    cb.unk9 = { 132.92885f,    66.40444f,     1000.0f,       1e-4f };
    cb.unk10 = { 131.92885f,    65.40444f,     55.853416f,    0.6784314f };
    cb.unk11 = { 131.92885f,    65.40444f,     999.0f,        5.5f };
    cb.unk12 = { 0.0f,          0.5f,          25.575994f,    0.0f };
    cb.unk13 = { 0.0f,          0.0f,          0.0f,          0.0f };
    cb.unk14 = { 0.025f,        10000.0f,      -9999.0f,      1.0f };
    cb.unk15 = { 1.0f,          1.0f,          1.0f,          0.0f };
    cb.unk16 = { 0.0f,          0.0f,          0.0f,          0.0f };
    cb.unk17 = { 10.979255f,    7.1482353f,    6.3034935f,    0.0f };
    cb.unk18 = { 0.0037614072f, 0.0f,          0.0f,          0.0f };
    cb.unk19 = { 0.0f,          0.0075296126f, 0.0f,          0.0f };
    cb.unk20 = { 0.0f,          0.0f,          0.017589089f,  0.0f };
    cb.unk21 = { 0.27266484f,  -0.31473818f,  -0.15603681f,   1.0f };
    cb.unk22 = { 0.0f,          0.0f,          0.0f,          0.0f };
    cb.unk23 = { 0.0f,          0.0f,          0.0f,          0.0f };
    cb.unk24 = { 0.0f,          0.0f,          0.0f,          0.0f };
    cb.unk25 = { 0.0f,          0.0f,          0.0f,          0.0f };
    cb.unk26 = { 0.0f,          0.0f,          0.0f,          0.0f };
    cb.unk27 = { 0.0f,          0.0f,          0.0f,          0.0f };
    cb.unk28 = { 0.0f,          0.0f,          0.0f,          0.0f };
    cb.unk29 = { 0.0f,          0.0f,          0.0f,          0.0f };
    cb.unk30 = { 0.0f,          0.0f,          0.0f,          0.0f };
    cb.unk31 = { 0.0f,          0.0f,          0.0f,          0.0f };
    cb.unk32 = { 0.0f,          0.0f,          0.0f,          0.0f };
    cb.unk33 = { 0.0f,          0.0f,          0.0f,          0.0f };
    cb.unk34 = { 0.0f,          0.0f,          0.0f,          0.0f };
    cb.unk35 = { 0.0f,          0.0f,          0.0f,          0.0f };
    cb.unk36 = { 1.0f,          0.0f,          0.0f,          0.0f };
    return cb;
}

template <class T>
static void UploadCBStruct(ID3D11DeviceContext* ctx, ID3D11Buffer* buf, const T& data) {
    if (!buf) return;
    D3D11_MAPPED_SUBRESOURCE m{};
    if (SUCCEEDED(ctx->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        std::memcpy(m.pData, &data, sizeof(T));
        ctx->Unmap(buf, 0);
    }
}

void PrintCbVS(const std::vector<Vec4>& cb_vs, const char* name = "cb_vs")
{
    char line[160];
    _snprintf_s(line, _TRUNCATE, "%s size = %zu\n", name, cb_vs.size());
    OutputDebugStringA(line);

    for (size_t i = 0; i < cb_vs.size(); ++i)
    {
        const auto& v = cb_vs[i];
        _snprintf_s(line, _TRUNCATE,
            "%s[%zu] = { %.6f, %.6f, %.6f, %.6f }\n",
            name, i, v.x, v.y, v.z, v.w);
        OutputDebugStringA(line);
    }
}


inline void CreateFrameAuxCB(ID3D11Device* dev, Microsoft::WRL::ComPtr<ID3D11Buffer>& cb)
{
    D3D11_BUFFER_DESC bd{};
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.ByteWidth = (UINT)sizeof(FrameAuxCB);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    FrameAuxCB init = MakeFrameAuxFromRustDefaults();
    D3D11_SUBRESOURCE_DATA srd{ &init, 0, 0 };
    dev->CreateBuffer(&bd, &srd, cb.ReleaseAndGetAddressOf());
}

inline void UpdateFrameAuxCB(ID3D11DeviceContext* ctx, ID3D11Buffer* cb, const FrameAuxCB& data)
{
    D3D11_MAPPED_SUBRESOURCE ms{};
    if (SUCCEEDED(ctx->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        std::memcpy(ms.pData, &data, sizeof(data));
        ctx->Unmap(cb, 0);
    }
}


void PrintCbVS_Console(const std::vector<Vec4>& cb_vs, const char* name = "cb_vs")
{
    printf("%s size = %zu\n", name, cb_vs.size());
    for (size_t i = 0; i < cb_vs.size(); ++i)
    {
        const auto& v = cb_vs[i];
        printf("%s[%zu] = { %.6f, %.6f, %.6f, %.6f }\n",
            name, i, v.x, v.y, v.z, v.w);
    }
}

template <class T>
static void UploadCB(ID3D11DeviceContext* ctx, ID3D11Buffer* buf, const std::vector<T>& src) {
    if (!buf) return;

    D3D11_MAPPED_SUBRESOURCE m{};
    HRESULT hr = ctx->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
    if (FAILED(hr)) {
        // log hr with DX debug layer; then bail
        ErrorLogger::Log(hr, "Failed to write to cbuffer");
        return;
    }

    const UINT dstBytes = src.size() * sizeof(T);
    const size_t srcBytes = src.size() * sizeof(T);
    const size_t n = std::min<size_t>(dstBytes, srcBytes);

    if (n) std::memcpy(m.pData, src.data(), n);
    if (dstBytes > n) std::memset(static_cast<uint8_t*>(m.pData) + n, 0, dstBytes - n);

    ctx->Unmap(buf, 0);
}

void TigerScope::LoadScopeInfo(ComPtr<ID3D11Device> pDevice, ComPtr<ID3D11DeviceContext> pContext)
{
    D3D11_BUFFER_DESC bd = {};
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    TagHash p = TagHash(Scope.stage_pixel.contstant_buffer.reference);
    bd.ByteWidth = p.size;
    bd.Usage = D3D11_USAGE_DYNAMIC;         // update from CPU
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    D3D11_SUBRESOURCE_DATA srd;
    srd.pSysMem = (const void*)p.data;
    srd.SysMemPitch = 0; srd.SysMemSlicePitch = 0;
    pDevice.Get()->CreateBuffer(&bd, nullptr, this->pscbuffer.GetAddressOf());
    D3D11_BUFFER_DESC bd_vs = {};
    bd_vs.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    TagHash v = TagHash(Scope.stage_vertex.contstant_buffer.reference);
    bd_vs.ByteWidth = v.size;
    bd_vs.Usage = D3D11_USAGE_DYNAMIC;         // update from CPU
    bd_vs.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    srd.pSysMem = (const void*)v.data;
    pDevice.Get()->CreateBuffer(&bd, nullptr, this->vscbuffer.GetAddressOf());
    D3D11_BUFFER_DESC bd_frame = {};
    bd_frame.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    TagHash v_frame = TagHash(Scope.stage_vertex.contstant_buffer.reference);
    bd_frame.ByteWidth = v_frame.size;
    bd_frame.Usage = D3D11_USAGE_DYNAMIC;         // update from CPU
    bd_frame.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    srd.pSysMem = (const void*)v_frame.data;
    pDevice.Get()->CreateBuffer(&bd, nullptr, this->frameAuxBuffer.GetAddressOf());
}

void TigerScope::UpdateScopeBuffers(ComPtr<ID3D11DeviceContext> pContext, ExternStorage& externs)
{
    // Pixel stage
    {
        if (this->Scope.stage_pixel.TFX_Bytecode.size() != 0) {
            TfxProgram prog = TfxProgram::FromBytecode(this->Scope.stage_pixel.TFX_Bytecode,
                this->Scope.stage_pixel.TFX_Constants);
            auto& cb = this->Scope.stage_pixel.SamplerFallback; // std::vector<Vec4>
            prog.Evaluate(externs, cb,{});                         // fills cb with Vec4s
            UploadCB(pContext.Get(), pscbuffer.Get(), cb);
        }
        
    }

    // Vertex stage
    {
        if (this->Scope.stage_vertex.TFX_Bytecode.size() != 0) {
            TfxProgram prog_vs = TfxProgram::FromBytecode(this->Scope.stage_vertex.TFX_Bytecode,
                this->Scope.stage_vertex.TFX_Constants);
            auto& cb_vs = this->Scope.stage_vertex.SamplerFallback; // std::vector<Vec4>

            prog_vs.Evaluate(externs, cb_vs,{});

            UploadCB(pContext.Get(), vscbuffer.Get(), cb_vs);
        }
    }
    if (this->Scope.name.name == "frame") {
        std::vector<Vec4> buffer;
        /*for (int i =0; i < 5; i++)
        {
            buffer.push_back(externs.getVec4(TfxExtern::Frame, i*0x10));
        }*/
        FrameAuxCB aux = MakeFrameAuxFromRustDefaults();
        aux.times.x = externs.getFloat(TfxExtern::Frame, 0);
        aux.times.y = externs.getFloat(TfxExtern::Frame, 0);
        aux.times.z = externs.getFloat(TfxExtern::Frame, 0x14);
        aux.times.w = 1.0f;
        UpdateFrameAuxCB(pContext.Get(), frameAuxBuffer.Get(), aux);
        //UploadCB(pContext.Get(), this->frameBuffer.Get(), buffer);

    }
    if (this->Scope.name.name == "transparent_advanced") {
        ScopeTransparentAdvancedCB ta = MakeScopeTransparentAdvancedFromRustDefaults();

        UploadCBStruct(pContext.Get(), this->pscbuffer.Get(), ta);
	}
}

void TigerScope::Bind(ComPtr<ID3D11DeviceContext> pContext)
{
    if (this->Scope.stage_vertex.contstant_buffer.hash != 0xffffffff)
        pContext->VSSetConstantBuffers(this->Scope.stage_vertex.constant_buffer_slot, 1, vscbuffer.GetAddressOf());
    if (this->Scope.stage_pixel.contstant_buffer.hash != 0xffffffff)
        pContext->PSSetConstantBuffers(this->Scope.stage_pixel.constant_buffer_slot, 1, pscbuffer.GetAddressOf());
    if (this->Scope.name.name == "frame") {
        //ID3D11Buffer* cb = externs.GetBuffer(TfxExtern::Frame);
        pContext->PSSetConstantBuffers(this->Scope.stage_pixel.constant_buffer_slot, 1, frameAuxBuffer.GetAddressOf());
        //pContext->PSSetConstantBuffers(this->Scope.stage_vertex.constant_buffer_slot, 1, frameAuxBuffer.GetAddressOf());
    }
}
