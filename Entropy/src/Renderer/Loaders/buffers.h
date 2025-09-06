#pragma once
#include <cstdint>

class buffers
{
};

struct IndexBufferHeader {
	int8_t unk0;
	int8_t is32;
	uint16_t unk02;
	uint32_t _zeros;
	uint64_t dataSize;
};

struct VertexBufferHeader {
	uint32_t dataSize;
	uint16_t stride;
	uint16_t vType;
};

class D2IndexBuffer
{
public:
	IndexBufferHeader header;
	unsigned char* data;
};

class D2VertexBuffer
{
public:
	VertexBufferHeader header;
	unsigned char* data;
};
