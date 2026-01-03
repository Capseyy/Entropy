#include "Renderer/Engine.h"
#include "Renderer/Graphics/Graphics.h"
#include "TigerEngine/package.h"
#include "TigerEngine/globaldata.h"
#include "TigerEngine/String/string.h"
#include <windows.h>
#include <cstdio>
#include "TigerEngine/ClientStartup/RenderGlobals.h"
#include "TigerEngine/Activity/activity.h"
#include <filesystem>
#include <ctime>
#include <string>
#include <iostream>
#include <fcntl.h>
#include <io.h>
#include "Config/AppConfig.h"

void EnsureConsole()
{
	
	if (GetConsoleWindow() == nullptr)
		AllocConsole();

	FILE* fp = nullptr;
	freopen_s(&fp, "CONOUT$", "w", stdout);
	freopen_s(&fp, "CONOUT$", "w", stderr);
	freopen_s(&fp, "CONIN$", "r", stdin);

	setvbuf(stdout, nullptr, _IOLBF, 0);
	setvbuf(stderr, nullptr, _IOLBF, 0);
}

static std::wstring MakeTimestampedLogName()
{
	std::time_t t = std::time(nullptr);
	std::tm tm{};
	localtime_s(&tm, &t);

	wchar_t buf[64];
	swprintf_s(buf, L"Entropy_%04d-%02d-%02d_%02d%02d%02d.log",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);

	return buf;
}

void RedirectStdoutStderrToLogFile()
{
	namespace fs = std::filesystem;

	wchar_t exePath[MAX_PATH]{};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);

	fs::path logDir = fs::path(exePath).parent_path() / L"logs";
	fs::create_directories(logDir);

	fs::path logPath = logDir / MakeTimestampedLogName();

	FILE* fpOut = nullptr;
	if (_wfreopen_s(&fpOut, logPath.c_str(), L"w", stdout) != 0 || !fpOut)
		return;

	FILE* fpErr = nullptr;
	_wfreopen_s(&fpErr, logPath.c_str(), L"a", stderr);

	setvbuf(stdout, nullptr, _IONBF, 0);
	setvbuf(stderr, nullptr, _IONBF, 0);

	std::fprintf(stdout, "==== Logging started ====\n");
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)

{
	RedirectStdoutStderrToLogFile();
	HRESULT hr = CoInitialize(NULL);
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to initialize COM library.");
		return -1;
	}

	AppConfig cfg;
	if (!LoadOrCreateConfig(cfg))
	{
		printf("Configuration cancelled. Exiting.\n");
		return 0;
	}
	const std::wstring packagesW = DerivePackagesFolder(cfg.game_root);
	if (!packagesW.empty())
	{
		SetPackagePath(WideToUtf8(packagesW));
		printf("Using packages path: %s\n", GetPackagePath().c_str());
	}
	auto RedactedKeys = Read_Redacted_Keys();
	auto pcache = GeneratePackageCache(RedactedKeys);
	for (const auto& pkg : pcache) {
		GlobalData::getMap().insert({ pkg.first, pkg.second });
		for (const auto& h64entry : pkg.second.h64s) {
			GlobalData::getH64().insert({ h64entry.hash64, TagHash(h64entry.hash32, true) });
		}
	}

	GetAllNamedTags();
	bool loadedRenderGlobals = GenerateRenderGlobals();
	printf("Loaded %zu packages\n", GlobalData::getMap().size());
	printf("Loaded %zu h64 entries\n", GlobalData::getH64().size());
	auto start = std::chrono::high_resolution_clock::now();
	auto StringMap = GenerateStringMap();
	for (auto& entry : StringMap) {
		GlobalData::globalString().insert(entry);
	}

	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed = end - start;
	GenerateTigerActivities();
	
	Engine engine;

	RECT workArea{};
	SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
	int w = workArea.right - workArea.left;
	int h = workArea.bottom - workArea.top;
	if (w <= 0 || h <= 0)
	{
		w = GetSystemMetrics(SM_CXSCREEN);
		h = GetSystemMetrics(SM_CYSCREEN);
	}

	if (engine.Initialize(hInstance, "Entropy", "EntropyEngineWindowClass", w, h))
	{
		while (engine.ProcessMessages() == true)
		{
			engine.Update();
			engine.RenderFrame();
		}
	}
	return 0;
}