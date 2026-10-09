#include "Config.hpp"
#include <soundphysics/Log.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

namespace spl::cfg {
namespace {

Settings gSettings{};

// Minimal JSON number/bool reader (no full parser dependency at runtime path scan)
bool findBool(const char* json, const char* key, bool def) {
    char pat[128];
    std::snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = std::strstr(json, pat);
    if (!p) return def;
    p = std::strchr(p + std::strlen(pat), ':');
    if (!p) return def;
    ++p;
    while (*p == ' ' || *p == '\t') ++p;
    if (std::strncmp(p, "true", 4) == 0) return true;
    if (std::strncmp(p, "false", 5) == 0) return false;
    return def;
}

float findFloat(const char* json, const char* key, float def) {
    char pat[128];
    std::snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = std::strstr(json, pat);
    if (!p) return def;
    p = std::strchr(p + std::strlen(pat), ':');
    if (!p) return def;
    ++p;
    while (*p == ' ' || *p == '\t') ++p;
    char* end = nullptr;
    float v = std::strtof(p, &end);
    if (end == p) return def;
    return v;
}

void applyJson(const char* json, Settings& s) {
    s.enabled = findBool(json, "enabled", s.enabled);
    s.debugLogging = findBool(json, "debug_logging", s.debugLogging);

    s.attenuationEnabled = findBool(json, "attenuation_enabled", s.attenuationEnabled);
    s.airAbsorptionEnabled = findBool(json, "air_absorption_enabled", s.airAbsorptionEnabled);
    s.soundOcclusionEnabled = findBool(json, "sound_occlusion_enabled", s.soundOcclusionEnabled);
    s.volumeOcclusionEnabled = findBool(json, "volume_occlusion_enabled", s.volumeOcclusionEnabled);
    s.roomReverbEnabled = findBool(json, "room_reverb_enabled", s.roomReverbEnabled);
    s.echoEnabled = findBool(json, "echo_enabled", s.echoEnabled);
    s.speedOfSoundEnabled = findBool(json, "speed_of_sound_enabled", s.speedOfSoundEnabled);

    s.attenuationFactor = findFloat(json, "attenuation_factor", s.attenuationFactor);
    s.airAbsorption = findFloat(json, "air_absorption", s.airAbsorption);
    s.maxOcclusion = findFloat(json, "max_occlusion", s.maxOcclusion);
    s.occlusionIntensity = findFloat(json, "occlusion_intensity", s.occlusionIntensity);
    s.volumeOcclusionIntensity = findFloat(json, "volume_occlusion_intensity", s.volumeOcclusionIntensity);
    s.reverbGain = findFloat(json, "reverb_gain", s.reverbGain);
    s.reverbBrightness = findFloat(json, "reverb_brightness", s.reverbBrightness);
    s.reverbDistance = findFloat(json, "reverb_distance", s.reverbDistance);
    s.echoIntensity = findFloat(json, "echo_intensity", s.echoIntensity);
    s.speedOfSound = findFloat(json, "speed_of_sound", s.speedOfSound);
    s.maxTravelDelaySec = findFloat(json, "max_travel_delay_sec", s.maxTravelDelaySec);
    s.maxSoundProcessingDistance = findFloat(json, "max_sound_processing_distance", s.maxSoundProcessingDistance);
    s.refDistance = findFloat(json, "ref_distance", s.refDistance);
    s.globalIntensity = findFloat(json, "global_intensity", s.globalIntensity);
}

} // namespace

Settings& get() { return gSettings; }

void resetDefaults() { gSettings = Settings{}; }

bool loadFromFile(const char* path) {
    if (!path) return false;
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 64 * 1024) {
        std::fclose(f);
        return false;
    }
    std::string buf(static_cast<size_t>(sz), '\0');
    if (std::fread(buf.data(), 1, static_cast<size_t>(sz), f) != static_cast<size_t>(sz)) {
        std::fclose(f);
        return false;
    }
    std::fclose(f);
    applyJson(buf.c_str(), gSettings);
    SPL_LOGI("Config loaded from %s (enabled=%d intensity=%.2f)", path, (int)gSettings.enabled,
             gSettings.globalIntensity);
    return true;
}

} // namespace spl::cfg
