#pragma once
#include "TigerEngine/map/static.h"
#include <vector>
#include <DirectXMath.h>
#include <span>
#include <stdexcept>
#include "static_loader.h"


class InstancedStatics
{
public:
	TagHash StaticTag;
	uint32_t InstanceCount;
	std::vector<DirectX::XMFLOAT4> RotationMatrcies;
	std::vector<DirectX::XMFLOAT3> TransformMatricies;
	std::vector<DirectX::XMFLOAT3> ScaleMatricies;
};


class StaticMap
{
public:
	void Initialize(TagHash StaticInstanceTable);
	void LoadStaticData();
	std::vector<InstancedStatics> instanced_statics;
};

