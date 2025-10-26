#include "Runtime/Assets/Technique.h"
#undef min
#undef max
#include "TigerEngine/Technique/Tfx/tfx_program.h"

bool EntropyAssets::Technique::Bind(Microsoft::WRL::ComPtr<ID3D11Device> pDevice, Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext) 
{
	TfxProgram prog = TfxProgram::FromBytecode(this->pixeldata.TFX_Bytecode, this->pixeldata.TFX_Constants);
	ExternStorage ex;
	auto& cb0 = this->pixeldata.SamplerFallback;
	//printf("Starting Technique %08X \n", this->id);
	//auto ops = ParseAll(this->pixeldata.TFX_Bytecode, /*trace=*/true);
	prog.Evaluate(ex, cb0);
	//auto pretty = prog.DecompilePretty();
	//printf("%s \n",pretty.c_str());
	if (!this->Textures.empty()) {
		//printf("Mapping Textures");
		const size_t n = std::min(this->Textures.size(), this->psTextureSlots.size());
		for (size_t i = 0; i < n; ++i) {
			UINT slot = this->psTextureSlots[i];
			ID3D11ShaderResourceView* s = this->Textures[i] ? this->Textures[i]->Get() : nullptr;
			pContext->PSSetShaderResources(slot, 1, &s);
		}
	}
	if (!this->Textures3D.empty()) {
		//printf("Mapping 3D Textures");
		const size_t n = std::min(this->Textures3D.size(), this->psTextureSlots3D.size());
		for (size_t i = 0; i < n; ++i) {
			UINT slot = this->psTextureSlots3D[i];
			ID3D11ShaderResourceView* s = this->Textures3D[i] ? this->Textures3D[i]->Get() : nullptr;
			pContext->PSSetShaderResources(slot, 1, &s);
		}
	}

	if (!this->CBuffers.empty()) {
		for (size_t i = 0; i < this->CBuffers.size(); ++i) {
			ID3D11Buffer* b = this->CBuffers[i]->buffer.Get();
			pContext->PSSetConstantBuffers(i, 1, &b);
		}
	}
	for (size_t i = 0; i < this->Samplers.size(); ++i) {
		ID3D11SamplerState* s = this->Samplers[i]->sampler.Get();
		pContext->PSSetSamplers(i + 1, 1, &s);
	}


	return true;
}