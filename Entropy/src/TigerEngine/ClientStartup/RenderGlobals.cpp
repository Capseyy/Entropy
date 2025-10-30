#include "RenderGlobals.h"


bool contains(const std::string& s, std::string_view needle) {
	return s.find(needle) != std::string::npos;
}


bool GenerateRenderGlobals()
{
	auto& namedtags = GlobalData::getNamedTags();
	TagHash renderglobals;
	bool foundRenderGlobals = false;
	for (auto& entry : namedtags)
	{
		if (entry.name == "render_globals" && contains(entry.tag.GetPackageName(), "client_startup"))
		{
			renderglobals = entry.tag;
			foundRenderGlobals = true;
			break;
		}
	}
	if (!foundRenderGlobals) {
		printf("Failed to find render globals\n");
		return false;
	}
	Unk_8080978C unk_parent = bin::parse<Unk_8080978C>(renderglobals.data, renderglobals.size, bin::Endian::Little);
	std::unordered_map<std::string, TagHash> globalTechs;
	for (auto& entry : unk_parent.entries)
	{
		if (entry.tag.reference == 0x808067A8) {
			auto render_globals = bin::parse<Unk_808067A8>(entry.tag.data, entry.tag.size, bin::Endian::Little);
			for (auto& tech : render_globals.techniques)
			{
				GlobalData::getGlobalTechniques().emplace(tech.name.name, tech.tech);
				printf("%s %08X \n", tech.name.name.c_str(), tech.tech.hash);
			}
			for (auto& tech : render_globals.scopes)
			{
				std::pair<std::string, TagHash> values;
				values.first = tech.name.name.c_str();
				values.second = tech.scope;
				GlobalData::getScopes().emplace_back(values);
				printf("Scope %s %08X \n", tech.name.name.c_str(), tech.scope.hash);
			}
		}

	}

	return true;
}