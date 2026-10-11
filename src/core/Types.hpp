#pragma once
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace sp {

struct Vec3 {
    float x = 0, y = 0, z = 0;
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 normalized() const {
        float l = length();
        if (l < 1e-6f) return {0, 0, 0};
        return {x / l, y / l, z / l};
    }
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
};

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// Result applied to one FMOD channel (Java SPR → C++)
struct AcousticResult {
    float volumeMul = 1.f;   // directGain
    float lowpassHz = 22000.f;
    float reverbSend = 0.f;  // aux / wet
    float pitchMul = 1.f;    // Doppler
    float occlusion = 0.f;   // 0..1 for debug
};

struct Config {
    bool enabled = true;

    // --- Occlusion (Sound Physics Remastered Java) ---
    bool occlusionEnabled = true;
    bool strictOcclusion = false;       // 1 path vs 9-variation
    int maxOcclusionRays = 16;          // blocks along path
    float maxOcclusion = 64.f;          // cap accumulation
    float globalBlockAbsorption = 1.f;  // absorptionCoeff
    float occlusionVolumeCut = 0.85f;   // volume drop strength
    float nonFullBlockFactor = 0.25f;
    float occlusionSmoothing = 0.35f;
    bool occlusionLowpass = true;
    float minLowpassHz = 800.f;
    float maxLowpassHz = 18000.f;
    float variationFactor = 0.25f;      // 9-ray offset scale

    // --- Environment / reverb (Java SPR simplified) ---
    bool reverbEnabled = true;
    int envRayCount = 16;               // Java default 32 — lower for mobile
    int envRayBounces = 3;
    float reverbGain = 1.f;
    float reverbSend = 0.55f;
    float reverbMaxWetDb = -6.f;
    float defaultReflectivity = 0.5f;

    // --- Echo ---
    bool echoEnabled = true;
    float echoDecay = 0.4f;

    // --- Doppler / distance ---
    bool dopplerEnabled = true;
    float speedOfSound = 343.f;         // blocks/sec (Java-style scale)
    float dopplerScale = 1.f;
    float maxDistance = 64.f;
    float attenuationFactor = 1.f;

    // --- Ray / block binding ---
    bool useWorldRay = true;            // try resolved getBlock
    float stepSize = 0.5f;              // voxel step when no native clip
};

extern Config g_cfg;
void loadConfig();
void saveConfig();

} // namespace sp
