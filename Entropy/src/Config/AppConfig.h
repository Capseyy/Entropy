#pragma once

#include <string>


struct AppConfig
{
	std::wstring game_root; 
};


bool LoadOrCreateConfig(AppConfig& outConfig);


std::wstring DerivePackagesFolder(const std::wstring& gameRoot);


std::string WideToUtf8(const std::wstring& w);
