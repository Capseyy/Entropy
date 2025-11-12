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
			std::pair<std::string, uint32_t> pair;
			pair.second = bubble_parent.parent.tagHash32;
			pair.first = bubble.bubble_string.string;
			bubbles.emplace_back(pair);
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
