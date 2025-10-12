#include <cstdint>
#include <vector>
#include <algorithm>
#include <cstring>
#include "TigerEngine/Technique/technique.h"
#include <dxgi.h>
#include <d3d11.h>
#include <wrl.h>
#include <cstdio>

#undef max
#undef min

struct STextureHeader {
public:
	uint32_t dataSize;
	uint32_t dxgiFormat;
	std::array<uint32_t, 6> _unk08;
	uint16_t cafe;
	uint16_t width;
	uint16_t height;
	uint16_t depth;
	uint16_t arraySize;
	uint16_t tileCount;
	uint8_t unk2c;
	uint8_t mipCount;
	std::array<uint8_t, 10> _unk2e;
	uint32_t unk38;
	TagHash large_buffer;
};

class TigerTexture
{
public:
	STextureHeader header;
	bool Initialize(ID3D11Device* device, TagHash textureTag);
	ID3D11ShaderResourceView* GetTexture();
	TagHash smallBuffer;
	uint32_t textureIndex;
private:
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureView;
};
