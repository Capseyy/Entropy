#include "static_loader.h"

bool StaticRenderer::Initialize(uint32_t staticTag)
{
	auto static_tag = TagHash(staticTag,false);//6E9E0281
	this->static_tag = static_tag;
	this->static_tag.getData();
	return true;
}

void StaticRenderer::Process()
{
	auto static_struct = bin::parse<SStaticModel>(static_tag.data, static_tag.size, bin::Endian::Little);
	auto mesh_tag = TagHash(static_struct.opaque_meshes);
	auto mesh_struct = bin::parse<SStaticMeshData>(mesh_tag.data, mesh_tag.size, bin::Endian::Little);
	int max_detail = 0xff;;
	for (auto group : mesh_struct.mesh_groups) {
		if (group.TfxRenderStage < max_detail) {
			max_detail = group.TfxRenderStage;
			printf("INPUT LAYOUT %d\n", group.input_layout_index);
			auto input_layout = INPUT_LAYOUTS[group.input_layout_index];
			printf("Input Layout with %zu elements\n", input_layout.elements.size());
		}
	}
	for (const auto& buffer_group : mesh_struct.buffers) {
		StaticBuffers sb;

		if (buffer_group.IndexBuffer.hash != 0xffffffff) {
			auto ibh=bin::parse<IndexBufferHeader>(buffer_group.IndexBuffer.data, buffer_group.IndexBuffer.size, bin::Endian::Little);
			auto ib_buffer = TagHash(buffer_group.IndexBuffer.reference).data;
			sb.indexBuffer.data = ib_buffer;
			sb.indexBuffer.header = ibh;
		}
		if (buffer_group.VertexBuffer.hash != 0xffffffff) {
			auto vbh=bin::parse<VertexBufferHeader>(buffer_group.VertexBuffer.data, buffer_group.VertexBuffer.size, bin::Endian::Little);
			auto vb_buffer = TagHash(buffer_group.VertexBuffer.reference).data;
			sb.vertexBuffer.data = vb_buffer;
			sb.vertexBuffer.header = vbh;
		}
		if (buffer_group.UVBuffer.hash != 0xffffffff) {
			auto uvbh=bin::parse<VertexBufferHeader>(buffer_group.UVBuffer.data, buffer_group.UVBuffer.size, bin::Endian::Little);
			auto uv_buffer = TagHash(buffer_group.UVBuffer.reference).data;
			sb.uvBuffer.data = uv_buffer;
			sb.uvBuffer.header = uvbh;
		}
		if (buffer_group.VertexColourBuffer.hash != 0xffffffff) {
			auto vcbh=bin::parse<VertexBufferHeader>(buffer_group.VertexColourBuffer.data, buffer_group.VertexColourBuffer.size, bin::Endian::Little);
			auto vc_buffer = TagHash(buffer_group.VertexColourBuffer.reference).data;
			sb.vertexColourBuffer.data = vc_buffer;
			sb.vertexColourBuffer.header = vcbh;
		}
		buffers.push_back(sb);
	}
	int MatIndex = 0;
	for (auto meshGroup : mesh_struct.mesh_groups) {
		if (meshGroup.TfxRenderStage > max_detail) {
			MatIndex++;
			continue;
		}
		StaticPart sp;
		sp.index_start = mesh_struct.parts[meshGroup.part_index].index_start;
		sp.index_count = mesh_struct.parts[meshGroup.part_index].index_count;
		sp.buffer_index = mesh_struct.parts[meshGroup.part_index].buffer_index;
		sp.LodCatagory = mesh_struct.parts[meshGroup.part_index].LodCatagory;
		sp.PrimitiveType = mesh_struct.parts[meshGroup.part_index].PrimitiveType;
		auto technique_tag = TagHash(static_struct.Techniques[MatIndex].Unk0);
		auto Technique = bin::parse<STechnique>(technique_tag.data, technique_tag.size, bin::Endian::Little);
		//Technique.Initialize();
		sp.input_layout_index = meshGroup.input_layout_index;
		MatIndex++;
		parts.push_back(sp);
	}
	printf("Processed static with %zu buffers and %zu parts\n", buffers.size(), parts.size());


	
}