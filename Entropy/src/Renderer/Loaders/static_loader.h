#pragma once
#include "TigerEngine/Map/static.h"
#include "TigerEngine/tag.h"
#include "Renderer/Loaders/buffers.h"
#include "TigerEngine/Technique/input_layout.h"
#include "TigerEngine/Technique/technique.h"


struct StaticBuffers {
	D2IndexBuffer indexBuffer;
	D2VertexBuffer vertexBuffer;
	D2VertexBuffer uvBuffer;
	D2VertexBuffer vertexColourBuffer;
};

struct StaticPart {
	uint32_t index_start;
	uint32_t index_count;
	uint8_t buffer_index;
	uint8_t LodCatagory;
	uint8_t PrimitiveType;
	TagHash material;
	uint8_t input_layout_index;
};

class StaticRenderer
{
public:
	bool Initialize(uint32_t staticTag);
	void Process();
	TagHash static_tag;
	std::vector<StaticBuffers> buffers;
	std::vector< StaticPart> parts;
};