#pragma once
#include "TigerEngine/tag.h"
#include "TigerEngine/String/string.h"
#include <string>
#include "TigerEngine/package.h"

struct TigerActivityPhase {
	uint32_t parent_hash;
	std::string name;
	uint32_t bubble_hash;
};

struct TigerBubble {
	uint32_t parent_hash;
	std::string name;
	uint32_t bubble_hash;
};


class TigerActivity {
public:
	uint32_t id;
	std::string dev_name;
	std::vector<TigerBubble> bubbles;
	std::vector<TigerActivityPhase> phases;
	void ProcessActivity(uint32_t activity_hash);
};

struct Unk_80808EBE {
	uint64_t FileSize;
	std::vector<uint32_t> activity_resource;
};

struct CommonActivityValues {
	uint32_t unk0;
	uint32_t unk4;
	uint64_t unk8;
	std::array<uint32_t, 6> hashes;
	uint32_t fnvHash;
	uint32_t unk2C;
	uint64_t world_id;
};

struct Unk_80804692 {
	uint64_t unk0;
	WideHash entity_data_table;
	StringHash entity_name;
	std::array<uint32_t, 5> unk;
};

struct Unk_80804690 {
	uint64_t unk0;
	uint64_t unk8;
	std::vector<Unk_80804692> combatant_instances;
};

struct Unk_8080462D {
	uint64_t unk0;
	WideHash entity_data_table;
	StringHash entity_name;
	std::array<uint32_t, 5> unk;
};

struct Unk_8080462B {
	uint64_t unk0;
	uint64_t unk8;
	std::vector<Unk_8080462D> combatant_instances;
};

struct Unk_808092D8 {
	CommonActivityValues common_values;
	std::array<uint32_t, 19> unk38;
	TagHash data_table;
};

struct Unk_80808943 {
	uint64_t FileSize;
	uint32_t phase_hash;
	std::array<uint32_t, 5> unk8;
	TagHash activity_resource_tag;
};

struct Unk_8080448B {
	std::array<uint32_t, 12> unk0;
	TagHash data_table;
};

struct Unk_80809956 {
	std::array<uint32_t, 4> unk0;
	ResourcePointer pointer;
};

struct Unk_80804695 {
	CommonActivityValues common_values;
	std::array<uint64_t, 9> unk38;
	std::vector<Unk_80809956> spawn_points;
	std::array<uint32_t, 7> unk68;
};

struct Unk_80809905 {
	uint32_t fnvHash;
	uint32_t unk2C;
	uint64_t world_id;
};


struct Unk_80809928 {
	uint64_t world_id;
	uint32_t unk08;
	uint32_t unk0c;
};

struct Unk_80804696 {
	std::array<uint32_t, 10> unk0;
	WideHash entity_data_table;
	StringHash entity_name;
	std::array<uint32_t, 11> unk20;
	ResourcePointer unk68;
	std::array<uint32_t, 4> unk70; 

};

struct Unk_808046B5 {
	CommonActivityValues common_values;
	std::array<uint64_t, 9> unk38;
	std::vector<Unk_80804696> combatant_instances;
	std::array<uint32_t, 8> unk68;
	uint64_t sr_id;
	uint64_t unkb8;
	glm::quat default_rot;
	glm::vec4 default_pos;
};

struct Unk_80804699 {
	CommonActivityValues common_values;
	std::array<uint64_t, 10> unk38;
	std::vector<Unk_80809928> spawn_rule_ids;
};


struct Unk_808098EF {
	CommonActivityValues common_values;
	std::array<uint64_t, 4> unk38;
	std::vector<Unk_80809905> object_groups;
	uint64_t unk68;
	RawStringPointer64 script_name;
};

struct Unk_80808CF8 {
	CommonActivityValues common_values;
	std::array<uint64_t, 4> unk38;
	std::vector<Unk_80809905> object_groups;
};


struct Unk_808098FA {
	CommonActivityValues common_values;
	std::array<uint32_t, 8> unk38;
	std::vector<Unk_80809905> object_groups;
	uint64_t unk68;
	RawStringPointer64 script_name;
};

struct Unk_80809d02 {
	ResourcePointer Unk0;
	ResourcePointer Unk8;
};

struct Unk_8080894D {
	RawStringPointer64 Unk0;
};

struct Unk_8080906b {
	uint64_t FileSize;
	std::vector<Unk_80809d02> string_table;
};

struct SActivityResource { 
	uint64_t FileSize;
	ResourcePointer unk8;
	ResourcePointer unk10;
	ResourcePointer unk18;
	SkipTo<0x80> UnkSkip;
	TagHash NameFile;
};

struct Unk_80808E89 {
	uint64_t FileSize;
	uint64_t unk8;
	ResourcePointer unk10;
	TagHash resource_table;
	
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
	ResourcePointer unk10; 
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
