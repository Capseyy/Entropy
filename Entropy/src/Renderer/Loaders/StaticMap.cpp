#include "StaticMap.h"


void StaticMap::Initialize(TagHash StaticInstanceTable)
{
	auto static_instancer = bin::parse<SStaticMeshInstances>(StaticInstanceTable.data, StaticInstanceTable.size, bin::Endian::Little);
	printf("Static Instance Table has %zu static tags\n", static_instancer.static_tags.size());
	for (auto& static_instance : static_instancer.instance_groups) {
		InstancedStatics instanedElements;
		instanedElements.InstanceCount = static_instance.instance_count;
		instanedElements.StaticTag = static_instancer.static_tags[static_instance.static_intex];
		for (int i = static_instance.instance_start; i < static_instance.instance_start + static_instance.instance_count; i++) {
			auto& transform = static_instancer.instance_transforms[i];
			DirectX::XMFLOAT4 rotationMatrix;
			rotationMatrix.x = transform.rotation[0];
			rotationMatrix.y = transform.rotation[1];
			rotationMatrix.z = transform.rotation[2];
			rotationMatrix.w = transform.rotation[3];
			instanedElements.RotationMatrcies.push_back(rotationMatrix);
			DirectX::XMFLOAT3 transformMatrix;
			transformMatrix.x = transform.translation[0];
			transformMatrix.y = transform.translation[1];
			transformMatrix.z = transform.translation[2];
			instanedElements.TransformMatricies.push_back(transformMatrix);
			DirectX::XMFLOAT3 scaleMatrix;
			scaleMatrix.x = transform.scale[0];
			scaleMatrix.y = transform.scale[1];
			scaleMatrix.z = transform.scale[2];
			instanedElements.ScaleMatricies.push_back(scaleMatrix);
		}
		this->instanced_statics.push_back(instanedElements);
	}
}

void StaticMap::LoadStaticData()
{
	for (auto& instanced_static : this->instanced_statics) {
		StaticRenderer static_renderer;
		if (static_renderer.Initialize(instanced_static.StaticTag.hash)) {
			static_renderer.ProcessFast();
		}
	}
	printf("Loaded %zu instanced statics\n", this->instanced_statics.size());
}