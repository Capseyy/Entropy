#include "Scope.h"
#include "TigerEngine/Technique/Tfx/tfx_program.h"

ComPtr<ID3D11Buffer> TigerScope::GetPSCBuffer() {
	return this->pscbuffer;
}
ComPtr<ID3D11Buffer> TigerScope::GetVSCBuffer() {
	return this->vscbuffer;
}

inline FrameAuxCB MakeFrameAuxFromRustDefaults() { return FrameAuxCB{}; }

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
        TfxProgram prog = TfxProgram::FromBytecode(this->Scope.stage_pixel.TFX_Bytecode,
            this->Scope.stage_pixel.TFX_Constants);
        auto& cb = this->Scope.stage_pixel.SamplerFallback; // std::vector<Vec4>
        prog.Evaluate(externs, cb);                         // fills cb with Vec4s
        UploadCB(pContext.Get(), pscbuffer.Get(), cb);
    }

    // Vertex stage
    {
        TfxProgram prog_vs = TfxProgram::FromBytecode(this->Scope.stage_vertex.TFX_Bytecode,
            this->Scope.stage_vertex.TFX_Constants);
        auto& cb_vs = this->Scope.stage_vertex.SamplerFallback; // std::vector<Vec4>
       
        prog_vs.Evaluate(externs, cb_vs);
            
        UploadCB(pContext.Get(), vscbuffer.Get(), cb_vs);
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
    }
}
