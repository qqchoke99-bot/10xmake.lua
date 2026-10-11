#include "core/Types.hpp"
#include <pl/ModMenu.hpp>
#include <string>

namespace sp {
extern Config g_cfg;
void saveConfig();
}

namespace sp::menu {

static constexpr const char* kModId = "sp.main";
static constexpr const char* kModName = "Sound Physics";

void onToggle(std::string_view, bool on) {
    sp::g_cfg.enabled = on;
    sp::saveConfig();
}

void onCfg(std::string_view, std::string_view key, std::string_view value) {
    auto& c = sp::g_cfg;
    const std::string k{key};
    const std::string v{value};
    try {
        auto isTrue = [&] { return v == "true" || v == "1"; };
        if (k == "occlusionEnabled") c.occlusionEnabled = isTrue();
        else if (k == "strictOcclusion") c.strictOcclusion = isTrue();
        else if (k == "occlusionStrength") c.globalBlockAbsorption = std::stof(v);
        else if (k == "occlusionVolumeCut") c.occlusionVolumeCut = std::stof(v);
        else if (k == "occlusionLowpass") c.occlusionLowpass = isTrue();
        else if (k == "reverbEnabled") c.reverbEnabled = isTrue();
        else if (k == "reverbSend") c.reverbSend = std::stof(v);
        else if (k == "echoEnabled") c.echoEnabled = isTrue();
        else if (k == "dopplerEnabled") c.dopplerEnabled = isTrue();
        else if (k == "speedOfSound") c.speedOfSound = std::stof(v);
        else if (k == "maxDistance") c.maxDistance = std::stof(v);
        else if (k == "maxOcclusionRays") c.maxOcclusionRays = std::stoi(v);
        sp::saveConfig();
    } catch (...) {
    }
}

static std::string f(float v) { return std::to_string(v); }

void registerAll() {
    auto& c = sp::g_cfg;
    using pl::modmenu::ConfigType;

    pl::modmenu::ModuleBuilder builder{std::string(kModId), std::string(kModName)};

    builder.description("Occlusion, reverb, echo, Doppler 343, lowpass (SPR formulas)")
        .defaultEnabled(c.enabled)
        .onToggle(onToggle)
        .onConfigChanged(onCfg);

    builder.config("occlusionEnabled", "Sound Occlusion", ConfigType::Toggle,
                   c.occlusionEnabled ? "true" : "false");
    builder.config("strictOcclusion", "Strict Occlusion", ConfigType::Toggle,
                   c.strictOcclusion ? "true" : "false");
    builder.config("occlusionStrength", "Block Absorption", ConfigType::SliderFloat,
                   f(c.globalBlockAbsorption), "0.1", "3.0");
    builder.config("occlusionVolumeCut", "Occlusion Volume", ConfigType::SliderFloat,
                   f(c.occlusionVolumeCut), "0.0", "1.0");
    builder.config("occlusionLowpass", "Occlusion Lowpass", ConfigType::Toggle,
                   c.occlusionLowpass ? "true" : "false");
    builder.config("reverbEnabled", "Room Reverb", ConfigType::Toggle,
                   c.reverbEnabled ? "true" : "false");
    builder.config("reverbSend", "Reverb Send", ConfigType::SliderFloat,
                   f(c.reverbSend), "0.0", "1.0");
    builder.config("echoEnabled", "Echo", ConfigType::Toggle,
                   c.echoEnabled ? "true" : "false");
    builder.config("dopplerEnabled", "Doppler", ConfigType::Toggle,
                   c.dopplerEnabled ? "true" : "false");
    builder.config("speedOfSound", "Speed of Sound", ConfigType::SliderFloat,
                   f(c.speedOfSound), "100", "500");
    builder.config("maxDistance", "Max Distance", ConfigType::SliderFloat,
                   f(c.maxDistance), "8", "128");
    builder.config("maxOcclusionRays", "Max Occlusion Rays", ConfigType::SliderFloat,
                   f(static_cast<float>(c.maxOcclusionRays)), "4", "64");

    // Preloader API: registerModule() on the builder (no .build())
    (void)builder.registerModule();
}

void unregisterAll() {
    pl::modmenu::unregisterModule(kModId);
}

} // namespace sp::menu
