#include "TigerEngine/Technique/Tfx/tfx_program.h"
#include "Renderer/Graphics/ImGui/imgui.h"
#include <string>
#include <vector>
#include <cctype>
#include <sstream>

#pragma once

enum class TfxStage { Auto, Vertex, Pixel, Geometry, Compute };

static const char* StageName(TfxStage s) {
    switch (s) {
    case TfxStage::Auto: return "Auto";
    case TfxStage::Vertex: return "Vertex (VS)";
    case TfxStage::Pixel: return "Pixel (PS)";
    case TfxStage::Geometry: return "Geometry (GS)";
    case TfxStage::Compute: return "Compute (CS)";
    default: return "Auto";
    }
}

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)std::tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return -1;
}

static bool ParseHexBytes(const char* text, std::vector<uint8_t>& outBytes, std::string& err)
{
    outBytes.clear();
    err.clear();
    if (!text) { err = "null input"; return false; }

    int hi = -1;
    for (size_t i = 0; text[i] != 0; ++i) {
        char c = text[i];

        if (std::isspace((unsigned char)c) || c == ',' || c == ';' || c == ':')
            continue;

        if (c == '0' && text[i + 1] && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
            ++i;
            continue;
        }

        int n = hex_nibble(c);
        if (n < 0) {
            std::ostringstream oss;
            oss << "Invalid hex char '" << c << "' at index " << i;
            err = oss.str();
            return false;
        }

        if (hi < 0) hi = n;
        else {
            outBytes.push_back((uint8_t)((hi << 4) | n));
            hi = -1;
        }
    }

    if (hi >= 0) { err = "Odd number of hex nibbles."; return false; }
    if (outBytes.empty()) { err = "No bytes parsed."; return false; }
    return true;
}

static bool g_analyze_ok = true;
static std::string g_analyze_err;

void DrawTfxBytecodeInspectorUI()
{
    static bool open = true;
    if (!open) return;

    static char g_tfxHexBuf[64 * 1024] = {};
    static char g_tfxOutBuf[64 * 1024] = {};

    static std::vector<uint8_t> bytes;
    static std::vector<Vec4> constants; // empty for now

    ImGui::Begin("TFX Bytecode Inspector", &open);

    if (ImGui::Button("Paste Clipboard")) {
        const char* clip = ImGui::GetClipboardText();
        if (clip) {
            // Copy with truncation safety
            std::snprintf(g_tfxHexBuf, IM_ARRAYSIZE(g_tfxHexBuf), "%s", clip);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        g_tfxHexBuf[0] = 0;
        g_tfxOutBuf[0] = 0;
        bytes.clear();
    }

    ImGui::Spacing();
    ImGui::TextWrapped("Paste raw hex bytes (e.g. \"4C 05 04 04 ...\").");
    ImGui::InputTextMultiline("##tfxhex", g_tfxHexBuf, IM_ARRAYSIZE(g_tfxHexBuf),
        ImVec2(-1.0f, 180.0f),
        ImGuiInputTextFlags_AllowTabInput);

    if (ImGui::Button("Analyze")) {
        g_analyze_ok = true;
        g_analyze_err.clear();

        std::string err;
        if (!ParseHexBytes(g_tfxHexBuf, bytes, err)) {
            g_analyze_ok = false;
            g_analyze_err = "Parse error: " + err;
            std::snprintf(g_tfxOutBuf, IM_ARRAYSIZE(g_tfxOutBuf), "%s\n", g_analyze_err.c_str());
        }
        else {
            try {
                constants.clear();

                TfxProgram prog = TfxProgram::FromBytecode(bytes, constants, 0);

          
                ExternStorage externs{};
                std::vector<Vec4> regs(16);
                prog.Evaluate_Trace(externs, regs, {});

                std::ostringstream oss;
                oss << "Bytes: " << bytes.size() << "\n";
                oss << "Channels used (" << prog.channels.size() << "):\n";
                for (auto ch : prog.channels) oss << "  - " << (int)ch << "\n";

                oss << "\nRaw bytes (first 256):\n";
                size_t n = bytes.size() < 256 ? bytes.size() : 256;
                for (size_t i = 0; i < n; i += 16) {
                    for (size_t j = 0; j < 16 && (i + j) < n; ++j) {
                        char tmp[8];
                        std::snprintf(tmp, sizeof(tmp), "%02X ", bytes[i + j]);
                        oss << tmp;
                    }
                    oss << "\n";
                }

                const std::string out = oss.str();
                std::snprintf(g_tfxOutBuf, IM_ARRAYSIZE(g_tfxOutBuf), "%s", out.c_str());
            }
            catch (const std::exception& e) {
                g_analyze_ok = false;
                g_analyze_err = std::string("Analyze failed: ") + e.what();
                std::snprintf(g_tfxOutBuf, IM_ARRAYSIZE(g_tfxOutBuf), "%s\n", g_analyze_err.c_str());
            }
            catch (...) {
                g_analyze_ok = false;
                g_analyze_err = "Analyze failed: unknown exception";
                std::snprintf(g_tfxOutBuf, IM_ARRAYSIZE(g_tfxOutBuf), "%s\n", g_analyze_err.c_str());
            }
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Output:");
    ImGui::BeginChild("##tfxout", ImVec2(-1.0f, 260.0f), true);
    ImGui::TextUnformatted(g_tfxOutBuf);
    ImGui::EndChild();

    ImGui::End();
}