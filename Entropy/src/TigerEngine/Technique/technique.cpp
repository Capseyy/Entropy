#include "TigerEngine/Technique/technique.h"

void STechnique::Initialize() 
{
	if (PixelShader.ShaderTag.hash != 0xffffffff) {
		auto ps_struct = bin::parse<ConstantBufferHeader>(PixelShader.contstant_buffer.data, PixelShader.contstant_buffer.size, bin::Endian::Little);
		printf("Pixel Shader CB Size: %llu\n", ps_struct.FileSize);
	}
}