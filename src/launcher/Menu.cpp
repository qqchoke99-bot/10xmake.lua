#include "core/Types.hpp"
#include <pl/ModMenu.hpp>
#include <android/log.h>
#include <string>

namespace sp {
extern Config g_cfg;
void saveConfig();
}

namespace sp::menu {

static constexpr const char* kMod = "sp.main";

void onToggle(std::string_view, bool on) {
    sp::g_cfg.enabled = on;
    sp::saveConfig();
}

void onCfg(std::string_view, std::string_view key, std::string_view value) {
    auto& c = sp::g_cfg;
    const std::string k{key}, v{value};
    try {
        auto b = [&] { return v == "true" || v == "1"; };
        if (k == "occlusionEnabled") c.occlusionEnabled = b();
        else if (k == "strictOcclusion") c.strictOcclusion = b();
        else if (k == "occlusionStrength") c.globalBlockAbsorption = std::stof(v);
        else if (k == "occlusionVolumeCut") c.occlusionVolumeCut = std::stof(v);
        else if (k == "occlusionLowpass") c.occlusionLowpass = b();
        else if (k == "reverbEnabled") c.reverbEnabled = b();
        else if (k == "reverbSend") c.reverbSend = std::stof(v);
        else if (k == "echoEnabled") c.echoEnabled = b();
        else if (k == "dopplerEnabled") c.dopplerEnabled = b();
        else if (k == "speedOfSound") c.speedOfSound = std::stof(v);
        else if (k == "maxDistance") c.maxDistance = std::stof(v);
        else if (k == "maxOcclusionRays") c.maxOcclusionRays = std::stoi(v);
        sp::saveConfig();
    } catch (...) {}
}

void registerAll() {
    auto& c = sp::g_cfg;
    auto mb = pl::modmenu::ModuleBuilder(kMod, "Sound Physics");
    mb.modId("SoundPhysics")
        .description("SPR formulas: occlusion, reverb, echo, Doppler 343, lowpass")
        .defaultEnabled(true)
        .onToggle(onToggle)
        .onConfigChanged(onCfg)
        .config("occlusionEnabled", "Sound Occlusion", pl::modmenu::ConfigType::Toggle,
                c.occlusionEnabled ? "true" : "false")
        .config("strictOcclusion", "Strict Occlusion (1 path)", pl::modmenu::ConfigType::Toggle,
                c.strictOcclusion ? "true" : "false")
        .config("occlusionStrength", "Block Absorption", pl::modmenu::ConfigType::SliderFloat,
                std::to_string(c.globalBlockAbsorption), "0.1", "3.0")
        .config("occlusionVolumeCut", "Occlusion Volume", pl::modmenu::ConfigType::SliderFloat,
                std::to_string(c.occlusionVolumeCut), "0.0", "1.0")
        .config("occlusionLowpass", "Occlusion Lowpass", pl::modmenu::ConfigType::Toggle,
                c.occlusionLowpass ? "true" : "false")
        .config("reverbEnabled", "Room Reverb", pl::modmenu::ConfigType::Toggle,
                c.reverbEnabled ? "true" : "false")
        .config("reverbSend", "Reverb Send", pl::modmenu::ConfigType::SliderFloat,
                std::to_string(c.reverbSend), "0.0", "1.0")
        .config("echoEnabled", "Echo", pl::modmenu::ConfigType::Toggle,
                c.echoEnabled ? "true" : "false")
        .config("dopplerEnabled", "Doppler", pl::modmenu::ConfigType::Toggle,
                c.dopplerEnabled ? "true" : "false")
        .config("speedOfSound", "Speed of Sound", pl::modmenu::ConfigType::SliderFloat,
                std::to_string(c.speedOfSound), "100", "500")
        .config("maxDistance", "Max Distance", pl::modmenu::ConfigType::SliderFloat,
                std::to_string(c.maxDistance), "8", "128")
        .config("maxOcclusionRays", "Max Occlusion Rays", pl::modmenu::ConfigType::SliderFloat,
                std::to_string((float)c.maxOcclusionRays), "4", "64");
    pl::modmenu::registerModule(mb.build());
}

void unregisterAll() {
    pl::modmenu::unregisterModule(kMod);
}

} // namespace sp::menu
