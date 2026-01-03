#include "AppConfig.h"

#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;

static std::wstring GetConfigFilePath()
{
	PWSTR roaming = nullptr;
	std::wstring base;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming)) && roaming)
	{
		base = roaming;
		CoTaskMemFree(roaming);
	}
	else
	{
		
		base = L".";
	}

	fs::path p = fs::path(base) / L"Entropy" / L"config.json";
	return p.wstring();
}

static std::wstring NormalizeSlashes(std::wstring p)
{
	for (auto& ch : p)
	{
		if (ch == L'\\') ch = L'/';
	}
	return p;
}

static std::wstring DenormalizeSlashes(std::wstring p)
{
	for (auto& ch : p)
	{
		if (ch == L'/') ch = L'\\';
	}
	return p;
}

std::string WideToUtf8(const std::wstring& w)
{
	if (w.empty()) return {};
	int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string r(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), r.data(), sizeNeeded, nullptr, nullptr);
	return r;
}

static bool TryReadConfig(AppConfig& outConfig)
{
	const std::wstring cfgPath = GetConfigFilePath();
	if (!fs::exists(cfgPath)) return false;

	std::ifstream f(cfgPath, std::ios::binary);
	if (!f) return false;
	std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

	
	const std::string key = "\"game_root\"";
	const size_t kpos = s.find(key);
	if (kpos == std::string::npos) return false;
	const size_t colon = s.find(':', kpos + key.size());
	if (colon == std::string::npos) return false;
	const size_t firstQuote = s.find('"', colon);
	if (firstQuote == std::string::npos) return false;
	const size_t secondQuote = s.find('"', firstQuote + 1);
	if (secondQuote == std::string::npos) return false;
	std::string val = s.substr(firstQuote + 1, secondQuote - firstQuote - 1);

	
	int wlen = MultiByteToWideChar(CP_UTF8, 0, val.c_str(), (int)val.size(), nullptr, 0);
	std::wstring wv(wlen, 0);
	MultiByteToWideChar(CP_UTF8, 0, val.c_str(), (int)val.size(), wv.data(), wlen);
	outConfig.game_root = DenormalizeSlashes(wv);
	return !outConfig.game_root.empty();
}

static bool WriteConfig(const AppConfig& cfg)
{
	const std::wstring cfgPathW = GetConfigFilePath();
	fs::path cfgPath(cfgPathW);
	fs::create_directories(cfgPath.parent_path());

	const std::wstring normalized = NormalizeSlashes(cfg.game_root);
	const std::string normalizedUtf8 = WideToUtf8(normalized);

	std::ofstream f(cfgPathW, std::ios::binary | std::ios::trunc);
	if (!f) return false;
	f << "{\n  \"game_root\": \"" << normalizedUtf8 << "\"\n}\n";
	return true;
}

static std::wstring Trim(const std::wstring& s)
{
	size_t b = 0;
	while (b < s.size() && iswspace(s[b])) b++;
	size_t e = s.size();
	while (e > b && iswspace(s[e - 1])) e--;
	return s.substr(b, e - b);
}

static bool IsReadableDirectory(const std::filesystem::path& p)
{
	std::error_code ec;
	if (!std::filesystem::exists(p, ec) || ec) return false;
	if (!std::filesystem::is_directory(p, ec) || ec) return false;

	
	std::filesystem::directory_iterator it(p, ec);
	return !ec;
}


static std::wstring CanonicalizePath(const std::wstring& in)
{
	std::error_code ec;
	std::filesystem::path p(Trim(in));
	if (p.empty()) return {};

	
	p = std::filesystem::absolute(p, ec);
	if (ec) return {};

	
	p = std::filesystem::weakly_canonical(p, ec);
	if (ec) return p.wstring(); 

	return p.wstring();
}

static bool LooksLikePackagesFolder(const std::filesystem::path& packagesDir)
{
	if (!IsReadableDirectory(packagesDir)) return false;

	std::error_code ec;
	size_t fileCount = 0;

	for (const auto& entry : std::filesystem::directory_iterator(packagesDir, ec))
	{
		if (ec) return false; 
		if (!entry.is_regular_file(ec) || ec) continue;

		
		

		fileCount++;
		if (fileCount >= 5) 
			return true;
	}

	return false;
}

static bool PickFolder(std::wstring& outFolder)
{
	ComPtr<IFileDialog> pFileDialog;
	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileDialog));
	if (FAILED(hr)) return false;

	DWORD options;
	pFileDialog->GetOptions(&options);
	pFileDialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
	pFileDialog->SetTitle(L"Select your game install folder");

	hr = pFileDialog->Show(NULL);
	if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return false;
	if (FAILED(hr)) return false;

	ComPtr<IShellItem> pItem;
	hr = pFileDialog->GetResult(&pItem);
	if (FAILED(hr)) return false;

	PWSTR pszFilePath = nullptr;
	hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
	if (FAILED(hr) || !pszFilePath) return false;

	outFolder = pszFilePath;
	CoTaskMemFree(pszFilePath);
	return true;
}

std::wstring DerivePackagesFolder(const std::wstring& gameRoot)
{
	if (gameRoot.empty()) return {};
	std::filesystem::path root(CanonicalizePath(gameRoot));
	if (root.empty()) return {};


	if (_wcsicmp(root.filename().wstring().c_str(), L"packages") == 0)
	{
		if (LooksLikePackagesFolder(root))
			return root.wstring();
		return {};
	}

	
	std::filesystem::path packages = root / L"packages";
	if (LooksLikePackagesFolder(packages))
		return packages.wstring();

	return {};
}

bool LoadOrCreateConfig(AppConfig& outConfig)
{
	if (TryReadConfig(outConfig))
	{
		
		if (!DerivePackagesFolder(outConfig.game_root).empty())
			return true;
	}

	MessageBoxW(NULL,
		L"First launch: please locate your game install folder.\n\n"
		L"Example: ...\\steamapps\\common\\Destiny 2",
		L"Entropy - Setup",
		MB_OK | MB_ICONINFORMATION);

	while (true)
	{
		std::wstring selected;
		if (!PickFolder(selected))
			return false;

		AppConfig candidate;
		candidate.game_root = selected;
		if (!DerivePackagesFolder(candidate.game_root).empty())
		{
			outConfig = candidate;
			WriteConfig(outConfig);
			return true;
		}

		MessageBoxW(NULL,
			L"That folder doesn't look like a valid game install folder (missing a 'packages' subfolder).\n\n"
			L"Please select the game's root folder (e.g. ...\\Destiny 2) and try again.",
			L"Entropy - Setup",
			MB_OK | MB_ICONWARNING);
	}
}
