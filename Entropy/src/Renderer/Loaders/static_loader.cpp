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
	int max_detail = 0xff;
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
			sb.indexBuffer.TagInt = buffer_group.IndexBuffer.hash;
		}
		if (buffer_group.VertexBuffer.hash != 0xffffffff) {
			auto vbh=bin::parse<VertexBufferHeader>(buffer_group.VertexBuffer.data, buffer_group.VertexBuffer.size, bin::Endian::Little);
			auto vb_buffer = TagHash(buffer_group.VertexBuffer.reference).data;
			sb.vertexBuffer.data = vb_buffer;
			sb.vertexBuffer.header = vbh;
			sb.vertexBuffer.TagInt = buffer_group.VertexBuffer.hash;
		}
		if (buffer_group.UVBuffer.hash != 0xffffffff) {
			auto uvbh=bin::parse<VertexBufferHeader>(buffer_group.UVBuffer.data, buffer_group.UVBuffer.size, bin::Endian::Little);
			auto uv_buffer = TagHash(buffer_group.UVBuffer.reference).data;
			sb.uvBuffer.data = uv_buffer;
			sb.uvBuffer.header = uvbh;
			sb.uvBuffer.TagInt = buffer_group.UVBuffer.hash;
		}
		if (buffer_group.VertexColourBuffer.hash != 0xffffffff) {
			auto vcbh=bin::parse<VertexBufferHeader>(buffer_group.VertexColourBuffer.data, buffer_group.VertexColourBuffer.size, bin::Endian::Little);
			auto vc_buffer = TagHash(buffer_group.VertexColourBuffer.reference).data;
			sb.vertexColourBuffer.data = vc_buffer;
			sb.vertexColourBuffer.header = vcbh;
			sb.vertexColourBuffer.TagInt = buffer_group.VertexColourBuffer.hash;
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
		sp.input_layout_index = meshGroup.input_layout_index;
		sp.material = Technique;
		sp.mesh_offset = mesh_struct.mesh_offset;
		sp.mesh_scale = mesh_struct.mesh_scale;
		sp.texture_coordinate_scale = mesh_struct.texture_coordinate_scale;
		sp.texture_coordinate_offset = mesh_struct.texture_coordinate_offset;
		sp.max_colour_index = mesh_struct.max_colour_index;

		MatIndex++;
		if ((mesh_struct.parts[meshGroup.part_index].LodCatagory) == 1)
			parts.push_back(sp);
	}
	printf("Processed static with %zu buffers and %zu parts\n", buffers.size(), parts.size());
}

bool StaticRenderer::InitializeRender(ID3D11Device* device,ID3D11DeviceContext* deviceContext)
{
	for (auto& part : this->parts)
	{   //needs to load techniques seperately cos they are shared a lot
		auto input_desc = INPUT_LAYOUTS[part.input_layout_index];
		Microsoft::WRL::ComPtr<ID3D11InputLayout> outLayout;
		D3D11_INPUT_ELEMENT_DESC outDesc;
		HRESULT hr = CreateInputLayoutFromTigerLayout(device, input_desc, outLayout, outDesc);
		if (FAILED(hr))
			ErrorLogger::Log(hr, L"Failed CreateInputLayoutFromTigerLayout on return");
		part.inputLayout = outLayout;
		part.materialRender.vs.tag = TagHash(part.material.VertexShader.ShaderTag.reference);
		printf("VSs Tag: %08X\n", part.materialRender.vs.tag.hash);
		part.materialRender.ps.tag = TagHash(part.material.PixelShader.ShaderTag.reference);
		printf("PSs Tag: %08X\n", part.materialRender.ps.tag.hash);
		part.materialRender.vs.Initialize(device, &outDesc, input_desc.elements.size());
		part.materialRender.ps.Initialize(device);
		for (auto tex : part.material.PixelShader.Textures) {
			TigerTexture tigerTex;
			auto HeaderTag = TagHash(tex.Texture.tagHash32);
			auto smallTag = TagHash(HeaderTag.reference);
			STextureHeader th = bin::parse<STextureHeader>(HeaderTag.data, HeaderTag.size);
			tigerTex.textureIndex = tex.TextureIndex;
			tigerTex.smallBuffer = smallTag;
			tigerTex.header = th;
			tigerTex.Initialize(device, HeaderTag);
			if (part.material.PixelShader.contstant_buffer.hash != 0xffffffff)
			{
				part.materialRender.InitializeCBuffer(device, th.dataSize, TagHash(part.material.PixelShader.contstant_buffer.reference));
			}
			for (auto& samp : part.material.PixelShader.Samplers) {
				auto SamplerTag = TagHash(samp.sampler.reference);
				auto sampler = bin::parse<UT_SamplerRaw>(SamplerTag.data, SamplerTag.size);
				part.materialRender.InitializeSampler(device, sampler);
			}
			part.materialRender.ps_textures.push_back(tigerTex.GetTexture());
		}
			
		
	}
	this->buffers[0].indexBuffer.Initialize(device);
	this->buffers[0].vertexBuffer.Initialize(device);
	this->buffers[0].uvBuffer.Initialize(device);
		
	this->UpdateWorldMatrix();
	return true;

}

void StaticRenderer::UpdateWorldMatrix()
{
	this->worldMatrix = XMMatrixIdentity();
}