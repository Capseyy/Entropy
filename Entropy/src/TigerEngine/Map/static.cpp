#include "static.h"
#include "TigerEngine/RenderStruct/RenderStruct.h"




std::vector<VertexPNUTC> BuildVertices(const VertexData& src)
{
	size_t count = src.positions.size();
	std::vector<VertexPNUTC> vertices(count);

	for (size_t i = 0; i < count; ++i)
	{
		VertexPNUTC v{};

		v.position = src.positions[i];

		if (i < src.normals.size())
			v.normal = DirectX::XMFLOAT3(src.normals[i].x, src.normals[i].y, src.normals[i].z);
		else
			v.normal = DirectX::XMFLOAT3(0, 0, 0);

		v.uv = (i < src.uvs.size()) ? src.uvs[i] : DirectX::XMFLOAT2(0, 0);

		v.color = (i < src.colors.size()) ? src.colors[i] : DirectX::XMFLOAT4(1, 1, 1, 1);

		vertices[i] = v;
	}

	return vertices;
}

StaticRenderObject ProcessStaticMesh(TagHash tag) {
	printf("Processing Static Mesh: %08X\n", tag.hash);
	SStaticModel sm = bin::parse<SStaticModel>(tag.data, tag.size);
	SStaticMeshData smd = bin::parse<SStaticMeshData>(sm.opaque_meshes.data, sm.opaque_meshes.size);
	StaticRenderObject sro;
	std::vector<VertexP> object;
	std::vector<VertexHeader> vertexBuffers;
	std::vector<IndexHeader> indexBuffers;
	std::vector<VertexHeader> uvBuffers;
	std::vector<VertexHeader> vcolBuffers;
	printf("Mesh scale: %f\n", smd.mesh_scale);
	for (auto& buffer_group : smd.buffers) {
		VertexData vd;
		auto ib = bin::parse<IndexHeader>(buffer_group.IndexBuffer.data, buffer_group.IndexBuffer.size);
		auto vb = bin::parse<VertexHeader>(buffer_group.VertexBuffer.data, buffer_group.VertexBuffer.size);
		auto uv = bin::parse<VertexHeader>(buffer_group.UVBuffer.data, buffer_group.UVBuffer.size);
		auto vcols = bin::parse<VertexHeader>(buffer_group.VertexColourBuffer.data, buffer_group.VertexColourBuffer.size);
		vcols.process_buffer(buffer_group.VertexBuffer.reference, smd.mesh_offset, smd.mesh_scale, smd.texture_coordinate_scale, smd.texture_coordinate_offset);
		indexBuffers.push_back(ib);
		auto positions = vb.process_buffer(buffer_group.VertexBuffer.reference, smd.mesh_offset, smd.mesh_scale, smd.texture_coordinate_scale, smd.texture_coordinate_offset);
		auto uvs = uv.process_buffer(buffer_group.VertexBuffer.reference, smd.mesh_offset, smd.mesh_scale, smd.texture_coordinate_scale, smd.texture_coordinate_offset);
		auto vcolurs = vcols.process_buffer(buffer_group.VertexBuffer.reference, smd.mesh_offset, smd.mesh_scale, smd.texture_coordinate_scale, smd.texture_coordinate_offset);
		vd.positions = positions.positions;
		vd.normals = positions.normals;
		vd.uvs = uvs.uvs;
		vd.colors = vcolurs.colors;
		auto final_vertex = BuildVertices(vd);
		sro.buffers.push_back(final_vertex);
	}
	for (auto& part : smd.parts) {
		StaticRenderPart srp;
		auto faces = indexBuffers[part.buffer_index].process_buffer(part.PrimitiveType, part.index_start, part.index_count, smd.buffers[part.buffer_index].IndexBuffer.reference);
		srp.cubeIdx = faces;
		srp.faceCount = part.index_count;
		srp.buffer_index = part.buffer_index;
		sro.parts.push_back(srp);

	}
	int groupCount = 0;
	for (auto& mesh_group : smd.mesh_groups) {
		sro.parts[mesh_group.part_index].TfxRenderStage = mesh_group.TfxRenderStage;
		sro.parts[mesh_group.part_index].input_layout_index = mesh_group.input_layout_index;
		sro.parts[mesh_group.part_index].technique = sm.Techniques[groupCount].Unk0;
		sro.parts[mesh_group.part_index].LodCatagory = smd.parts[mesh_group.part_index].LodCatagory;
		groupCount++;
	}
	return sro;
}

