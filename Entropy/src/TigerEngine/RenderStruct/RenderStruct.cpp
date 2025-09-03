#include "RenderStruct.h"

std::vector<uint32_t> IndexHeader::process_buffer(uint8_t primitive_type, uint32_t index_start, uint32_t index_count, uint32_t hash) {
	TagHash buffer_tag(hash);
	std::vector<uint32_t> indices;
	auto buffer_data = buffer_tag.data;
	//printf("Index start: %d, index count: %d, primitive type: %d, is_32_bit: %d\n", index_start, index_count, primitive_type, is_32_bit);
	if (primitive_type == 3) {
		for (int i = 0; i < index_count; i += 3) { //triangle 3s
			//printf("Processing triangle %d\n", i / 3);
			if (is_32_bit) {
				uint32_t idx0 = *(uint32_t*)(buffer_data + (index_start + i) * 4);
				uint32_t idx1 = *(uint32_t*)(buffer_data + (index_start + i + 1) * 4);
				uint32_t idx2 = *(uint32_t*)(buffer_data + (index_start + i + 2) * 4);
				indices.push_back(idx0);
				indices.push_back(idx1);
				indices.push_back(idx2);
				//printf("Indices: %d, %d, %d\n", idx0, idx1, idx2);
			}
			else {
				uint16_t idx0 = *(uint16_t*)(buffer_data + (index_start + i) * 2);
				uint16_t idx1 = *(uint16_t*)(buffer_data + (index_start + i + 1) * 2);
				uint16_t idx2 = *(uint16_t*)(buffer_data + (index_start + i + 2) * 2);
				indices.push_back(idx0);
				indices.push_back(idx1);
				indices.push_back(idx2);
				 
				//printf("Indices: %d, %d, %d\n", idx0, idx1, idx2);
			}
		}
	}
	//std::cout << "Processed " << indices.size() << " indices for tag " << std::hex << hash << std::dec << std::endl;


	return indices;

}

VertexData VertexHeader::process_buffer(uint32_t hash, std::array<float_t, 3> mesh_offset, float_t mesh_scale, float_t texture_coordinate_scale, std::array<float_t, 2> texture_coordinate_offset) {
	VertexData data;
	TagHash buffer_tag(hash);
	auto buffer_data = buffer_tag.data;
	//printf("Vertex Buffer Size: %d, Stride: %d, Type: %d, Hash: %d\n", size, stride, type, hash);
	if (type == 5) {
		if (stride == 4) {
			for (int i = 0; i < size; i += 4) {
				uint8_t r = *(uint8_t*)(buffer_data + i + 0);
				uint8_t g = *(uint8_t*)(buffer_data + i + 1);
				uint8_t b = *(uint8_t*)(buffer_data + i + 2);
				uint8_t a = *(uint8_t*)(buffer_data + i + 3);
				data.colors.push_back(DirectX::XMFLOAT4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
			}
			return data;
		}
	}
	else {
		if (stride == 0x10) {
			printf("Processing Position/Normal buffer with size %d and scale %f \n", size, mesh_scale);
			for (int i = 0; i < size; i += 16) {
				int16_t x = *(int16_t*)(buffer_data + i + 0);
				int16_t y = *(int16_t*)(buffer_data + i + 2);
				int16_t z = *(int16_t*)(buffer_data + i + 4);
				int16_t w = *(int16_t*)(buffer_data + i + 6);
				int16_t t1 = *(int16_t*)(buffer_data + i + 8);
				int16_t t2 = *(int16_t*)(buffer_data + i + 10);
				int16_t t3 = *(int16_t*)(buffer_data + i + 12);
				int16_t t4 = *(int16_t*)(buffer_data + i + 14);
				data.positions.push_back(
					DirectX::XMFLOAT4(
						(x / 32767.0f) * mesh_scale,
						(y / 32767.0f) * mesh_scale,
						(z / 32767.0f) * mesh_scale,
						(w / 32767.0f) * mesh_scale
					)
				);
				data.tangents.push_back(DirectX::XMFLOAT4(t1 / 32767.0f, t2 / 32767.0f, t3 / 32767.0f, t4 / 32767.0f));
			}
			return data;
		}
		else if (stride == 0x4) {
			printf("Processing UV buffer with size %d\n", size);
			printf("Texture Coordinate Scale: %f\n", texture_coordinate_scale);
			printf("Texture Coordinate Offset: %f, %f\n", texture_coordinate_offset[0], texture_coordinate_offset[1]);
			for (int i = 0; i + 3 < size; i += 4) {
				int16_t xi, yi;
				memcpy(&xi, buffer_data + i, sizeof(int16_t));
				memcpy(&yi, buffer_data + i + 2, sizeof(int16_t));
				float nx = static_cast<float>(xi) / 32767.0f;
				float ny = static_cast<float>(yi) / 32767.0f;
				float new_x = nx * texture_coordinate_scale + texture_coordinate_offset[0];
				float new_y = ny * -texture_coordinate_scale + 1.0f - texture_coordinate_offset[1];
				//printf("Raw UV: %.6f, %.6f\n", new_x, new_y);
				data.uvs.emplace_back(new_x, new_y);
			}
			return data;
		}
	}
}

std::vector<VertexPNUTC> BuildVertices(
	const std::vector<DirectX::XMFLOAT4>& positions,
	const std::vector<DirectX::XMFLOAT3>& normals,
	const std::vector<DirectX::XMFLOAT2>& uvs,
	const std::vector<DirectX::XMFLOAT4>& colors,
	const std::vector<DirectX::XMFLOAT4>& tangents
)
{
	size_t count = positions.size();
	std::vector<VertexPNUTC> vertices(count);

	for (size_t i = 0; i < count; ++i)
	{
		VertexPNUTC v{};
		v.position = positions[i];
		v.normal = (i < normals.size()) ? normals[i] : DirectX::XMFLOAT3(0, 0, 0);
		v.uv = (i < uvs.size()) ? uvs[i] : DirectX::XMFLOAT2(0, 0);
		v.color = (i < colors.size()) ? colors[i] : DirectX::XMFLOAT4(1, 1, 1, 1);

		vertices[i] = v;
	}

	return vertices;
}