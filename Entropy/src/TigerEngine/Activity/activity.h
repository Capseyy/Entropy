#pragma once
#include "TigerEngine/tag.h"
#include "TigerEngine/String/string.h"
#include <string>
#include "TigerEngine/package.h"

class TigerActivity {
public:
	uint32_t id;
	std::string dev_name;
	std::vector<std::pair<std::string, uint32_t>> bubbles;
	void ProcessActivity(uint32_t activity_hash);
};

struct Unk_80809700 {
	uint64_t unk0;
	uint32_t variable_fnv;
	uint32_t unk0c;
	ResourcePointer value;
};

struct Unk_80808948 {
	uint32_t unk0;
	uint32_t unk4;
	uint32_t destination_string;
	uint32_t phase_id;
	uint32_t phase_id2;
	TagHash activity_resource_parent;
};

struct Unk_8080891D
{
	WideHash parent;
};

struct Unk_80808924 {
	uint32_t unk0;
	uint32_t unk4;
	StringHash bubble_string;
	uint32_t unk0c;
	ResourcePointer unk10; //TODO
	std::vector<Unk_80808948> activity_resources;
	std::vector<Unk_8080891D> bubble_parents;
	std::array<uint32_t, 4> unk38;
};

struct Unk_8080bd7e {
	RawStringPointer64 unk_name;
	std::array<uint64_t, 8> unk08;
};

struct Unk_8080BD80 {
	RawStringPointer64 phase_name;
	std::array<uint64_t, 8> unk08;
	uint32_t unk48;
	uint32_t unk4c;


};

struct Unk_80808926 {
	uint32_t unk0;
	uint32_t unk4;
	uint32_t unk8;
	uint32_t phase_id;
	uint32_t unk10;
	uint32_t unk14;
	uint32_t unk18;
	uint32_t unk1c;
	uint32_t unk20;
	uint32_t unk24;
	uint32_t unk28;
	uint32_t unk2c;
	uint32_t unk30;
	uint32_t unk34;
	uint32_t unk38;
	uint32_t unk3c;
	uint32_t unk40;
	uint8_t flag1;
	uint8_t flag2;
	uint8_t flag3;
	uint8_t flag4;
	uint64_t unk48;
	std::vector<Unk_80808948> activity_resources;
	std::array<uint32_t, 4> unk60;

};

struct SActivity {
	uint64_t FileSize;
	uint32_t location_name;
	uint32_t unk0c;
	uint32_t unk10;
	uint32_t unk14;
	uint32_t unk18;
	uint32_t unk1c;
	WideHash destination;
	std::vector<Unk_80809700> activity_variables;
	std::vector<Unk_80808926> phase_resources;
	std::vector<Unk_80808924> bubble_tables;


};

void GenerateTigerActivities();
