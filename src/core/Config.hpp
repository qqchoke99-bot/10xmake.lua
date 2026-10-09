#pragma once

#include <string>

namespace spl::cfg {

// Mirrors Sound Physics Remastered (de.maxhenkel / Sonic Ether) settings,
// adapted to FMOD distance path (no world raycast yet).

struct Settings {
    // Master
    bool enabled = true;
    bool debugLogging = true;

    // ---- Feature toggles ----
    bool attenuationEnabled = true;
    bool airAbsorptionEnabled = true;   // lowpass with distance
    bool soundOcclusionEnabled = true;  // FMOD 3D occlusion
    bool volumeOcclusionEnabled = true;
    bool roomReverbEnabled = true;
    bool echoEnabled = true;
    bool speedOfSoundEnabled = true;

    // ---- Intensities (SPR-style names where possible) ----
    // attenuationFactor: 1.0 ≈ physical; higher = quieter with distance
    float attenuationFactor = 1.35f;
    // airAbsorption: 0..1 how fast high frequencies die
    float airAbsorption = 0.85f;
    // maxOcclusion: 0..1 cap for occlusion amount
    float maxOcclusion = 0.92f;
    // occlusion intensity multiplier
    float occlusionIntensity = 1.25f;
    // volume drop from "occlusion" proxy
    float volumeOcclusionIntensity = 1.0f;
    // reverbGain: overall reverb wet
    float reverbGain = 1.40f;
    // reverbBrightness: 0 dark muffled reverb .. 1 bright
    float reverbBrightness = 0.55f;
    // reverbDistance: scale of reverb vs sound distance
    float reverbDistance = 1.15f;
    // echo
    float echoIntensity = 1.0f;
    // speed of sound (blocks/s) — user mapping 343
    float speedOfSound = 343.0f;
    float maxTravelDelaySec = 0.45f;

    // Distance
    float maxSoundProcessingDistance = 72.0f;
    float refDistance = 8.0f; // near reference for attenuation curve

    // Global intensity master (multiplies all effect amounts)
    float globalIntensity = 1.25f;
};

Settings& get();
bool loadFromFile(const char* path);
void resetDefaults();

} // namespace spl::cfg
