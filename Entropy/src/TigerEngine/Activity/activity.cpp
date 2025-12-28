#include "activity.h"
#include "TigerEngine/globaldata.h"

void TigerActivity::ProcessActivity(uint32_t activity_hash) {
	auto Activity_Tag = TagHash(activity_hash);
	if (!Activity_Tag.success) {
		return;
	}
	auto activity_struct = bin::parse<SActivity>(Activity_Tag.data, Activity_Tag.size);
	for (const auto& bubble : activity_struct.bubble_tables) {
		for (auto& bubble_parent : bubble.bubble_parents) {
			TigerBubble tb;
			tb.parent_hash = bubble_parent.parent.tagHash32;
			tb.name = bubble.bubble_string.string;
			tb.bubble_hash = bubble.bubble_string.hash;
			bubbles.emplace_back(tb);
		}
		for (auto& activity_resource : bubble.activity_resources) {
			auto resource_parent = bin::parse<Unk_80808E89>(activity_resource.activity_resource_parent.data, activity_resource.activity_resource_parent.size);
			auto resource_table = bin::parse<Unk_80808EBE>(resource_parent.resource_table.data, resource_parent.resource_table.size);
			auto main_script = TagHash(resource_table.activity_resource[0]);
			auto script_data = bin::parse<Unk_80808943>(main_script.data, main_script.size);
			auto main_script_resource = bin::parse<SActivityResource>(script_data.activity_resource_tag.data, script_data.activity_resource_tag.size);
			if (main_script_resource.unk18.type == 0x808098FA) {
				TigerActivityPhase tap;
				tap.parent_hash = resource_parent.resource_table.hash;
				
				auto activity_data = main_script_resource.unk18.Parse<Unk_808098FA>(script_data.activity_resource_tag);
				tap.name = activity_data.script_name.name;
				tap.bubble_hash = bubble.bubble_string.hash;
				this->phases.emplace_back(tap);
				
			}
		}
		
	}
}

void GenerateTigerActivities() {
	auto& namedTags = GlobalData::getNamedTags();
	for (auto& tag : namedTags) {
		if (tag.reference == 0x80808E8E) {
			TigerActivity ta;
			ta.id = tag.tag.hash;
			ta.dev_name = tag.name;
			ta.ProcessActivity(static_cast<uint32_t>(tag.tag.hash));
			GlobalData::globalActivities().emplace_back(ta);
		}
	}
}
