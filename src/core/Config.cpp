#include "core/Types.hpp"
#include <android/log.h>
#include <fstream>
#include <sstream>
#include <string>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "SoundPhysics", __VA_ARGS__)

namespace sp {
Config g_cfg;

static std::string cfgPath() {
    return "/sdcard/games/SoundPhysics/config.json";
}

static void ensureDir() {
    // best-effort; Levi / Android may already have /sdcard/games
    system("mkdir -p /sdcard/games/SoundPhysics 2>/dev/null");
}

void loadConfig() {
    ensureDir();
    std::ifstream in(cfgPath());
    if (!in) {
        saveConfig();
        return;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string j = ss.str();
    auto getBool = [&](const char* k, bool def) {
        auto p = j.find(std::string("\"") + k + "\"");
        if (p == std::string::npos) return def;
        auto c = j.find(':', p);
        if (c == std::string::npos) return def;
        return j.find("true", c) < j.find("false", c);
    };
    auto getF = [&](const char* k, float def) {
        auto p = j.find(std::string("\"") + k + "\"");
        if (p == std::string::npos) return def;
        auto c = j.find(':', p);
        if (c == std::string::npos) return def;
        try { return std::stof(j.substr(c + 1)); } catch (...) { return def; }
    };
    auto getI = [&](const char* k, int def) {
        return (int)getF(k, (float)def);
    };
    auto& c = g_cfg;
    c.enabled = getBool("enabled", c.enabled);
    c.occlusionEnabled = getBool("occlusionEnabled", c.occlusionEnabled);
    c.strictOcclusion = getBool("strictOcclusion", c.strictOcclusion);
    c.maxOcclusionRays = getI("maxOcclusionRays", c.maxOcclusionRays);
    c.maxOcclusion = getF("maxOcclusion", c.maxOcclusion);
    c.globalBlockAbsorption = getF("globalBlockAbsorption", c.globalBlockAbsorption);
    c.occlusionVolumeCut = getF("occlusionVolumeCut", c.occlusionVolumeCut);
    c.nonFullBlockFactor = getF("nonFullBlockFactor", c.nonFullBlockFactor);
    c.occlusionSmoothing = getF("occlusionSmoothing", c.occlusionSmoothing);
    c.occlusionLowpass = getBool("occlusionLowpass", c.occlusionLowpass);
    c.minLowpassHz = getF("minLowpassHz", c.minLowpassHz);
    c.maxLowpassHz = getF("maxLowpassHz", c.maxLowpassHz);
    c.reverbEnabled = getBool("reverbEnabled", c.reverbEnabled);
    c.envRayCount = getI("envRayCount", c.envRayCount);
    c.envRayBounces = getI("envRayBounces", c.envRayBounces);
    c.reverbGain = getF("reverbGain", c.reverbGain);
    c.reverbSend = getF("reverbSend", c.reverbSend);
    c.echoEnabled = getBool("echoEnabled", c.echoEnabled);
    c.echoDecay = getF("echoDecay", c.echoDecay);
    c.dopplerEnabled = getBool("dopplerEnabled", c.dopplerEnabled);
    c.speedOfSound = getF("speedOfSound", c.speedOfSound);
    c.dopplerScale = getF("dopplerScale", c.dopplerScale);
    c.maxDistance = getF("maxDistance", c.maxDistance);
    c.useWorldRay = getBool("useWorldRay", c.useWorldRay);
    LOGI("config loaded path=%s", cfgPath().c_str());
}

void saveConfig() {
    ensureDir();
    const auto& c = g_cfg;
    std::ofstream out(cfgPath());
    if (!out) return;
    out << "{\n"
        << "  \"enabled\": " << (c.enabled ? "true" : "false") << ",\n"
        << "  \"occlusionEnabled\": " << (c.occlusionEnabled ? "true" : "false") << ",\n"
        << "  \"strictOcclusion\": " << (c.strictOcclusion ? "true" : "false") << ",\n"
        << "  \"maxOcclusionRays\": " << c.maxOcclusionRays << ",\n"
        << "  \"maxOcclusion\": " << c.maxOcclusion << ",\n"
        << "  \"globalBlockAbsorption\": " << c.globalBlockAbsorption << ",\n"
        << "  \"occlusionVolumeCut\": " << c.occlusionVolumeCut << ",\n"
        << "  \"occlusionLowpass\": " << (c.occlusionLowpass ? "true" : "false") << ",\n"
        << "  \"minLowpassHz\": " << c.minLowpassHz << ",\n"
        << "  \"maxLowpassHz\": " << c.maxLowpassHz << ",\n"
        << "  \"reverbEnabled\": " << (c.reverbEnabled ? "true" : "false") << ",\n"
        << "  \"envRayCount\": " << c.envRayCount << ",\n"
        << "  \"reverbGain\": " << c.reverbGain << ",\n"
        << "  \"reverbSend\": " << c.reverbSend << ",\n"
        << "  \"echoEnabled\": " << (c.echoEnabled ? "true" : "false") << ",\n"
        << "  \"echoDecay\": " << c.echoDecay << ",\n"
        << "  \"dopplerEnabled\": " << (c.dopplerEnabled ? "true" : "false") << ",\n"
        << "  \"speedOfSound\": " << c.speedOfSound << ",\n"
        << "  \"maxDistance\": " << c.maxDistance << ",\n"
        << "  \"useWorldRay\": " << (c.useWorldRay ? "true" : "false") << "\n"
        << "}\n";
}
} // namespace sp
