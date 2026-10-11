#include "occ/Occlusion.hpp"
#include "mc/Raycast.hpp"
#include "core/Types.hpp"
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace sp::occ {
namespace {

struct Smooth {
    float occ = 0, vol = 1, lp = 22000, rev = 0;
};

std::mutex g_mtx;
std::unordered_map<uint64_t, Smooth> g_smooth;

uint64_t keyOf(const Vec3& s) {
    auto q = [](float v) -> int { return (int)std::lround(v * 4.f); };
    return (uint64_t)(uint32_t)q(s.x) | ((uint64_t)(uint32_t)q(s.y) << 20) |
           ((uint64_t)(uint32_t)q(s.z) << 40);
}

// --- Java: runOcclusion(soundPos, playerPos) ---
float runOcclusion(const Vec3& sound, const Vec3& player) {
    if (!sp::g_cfg.occlusionEnabled) return 0.f;
    // March sound → player, accumulate block cost (maxOcclusionRays steps)
    float acc = sp::mc::rayCost(sound, player);
    return std::min(acc, sp::g_cfg.maxOcclusion);
}

// --- Java: calculateOcclusion with optional 9-path variation ---
float calculateOcclusion(const Vec3& sound, const Vec3& player) {
    if (sp::g_cfg.strictOcclusion) {
        return runOcclusion(sound, player);
    }
    // 9-variation (simplified axis offsets) like Java variationFactor loop
    const float vf = sp::g_cfg.variationFactor;
    float best = runOcclusion(sound, player);
    static const float offs[8][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0},
        {0, 0, 1}, {0, 0, -1}, {0.7f, 0.7f, 0}, {-0.7f, 0.7f, 0},
    };
    for (auto& o : offs) {
        Vec3 a{sound.x + o[0] * vf, sound.y + o[1] * vf, sound.z + o[2] * vf};
        Vec3 b{player.x + o[0] * vf, player.y + o[1] * vf, player.z + o[2] * vf};
        best = std::min(best, runOcclusion(a, b));
    }
    return std::min(best, sp::g_cfg.maxOcclusion);
}

// Environment rays for reverb (Java numRays × bounces, mobile-reduced)
float evaluateReverbSend(const Vec3& sound) {
    if (!sp::g_cfg.reverbEnabled) return 0.f;
    const int n = std::max(4, std::min(sp::g_cfg.envRayCount, 32));
    // Fibonacci-ish directions
    float reflectAcc = 0.f;
    int hits = 0;
    const float maxD = 16.f;
    for (int i = 0; i < n; ++i) {
        float t = (float)i / (float)n;
        float y = 1.f - 2.f * t;
        float r = std::sqrt(std::max(0.f, 1.f - y * y));
        float th = 2.399963f * (float)i; // golden angle
        Vec3 dir{r * std::cos(th), y, r * std::sin(th)};
        Vec3 end = sound + dir * maxD;
        auto hit = sp::mc::rayMarch(sound, end, 12);
        if (hit.hit) {
            reflectAcc += hit.reflectivity / (1.f + 0.08f * hit.distance);
            ++hits;
        }
    }
    float room = hits > 0 ? (reflectAcc / float(n)) : 0.05f;
    // open sky (few hits) → less wet
    float open = 1.f - float(hits) / float(n);
    room *= (1.f - 0.7f * open);
    return clampf(room * sp::g_cfg.reverbSend * sp::g_cfg.reverbGain, 0.f, 1.f);
}

} // namespace

AcousticResult evaluate(const Vec3& listener, const Vec3& source,
                        const Vec3& listenerVel, const Vec3& sourceVel) {
    AcousticResult r;
    if (!sp::g_cfg.enabled) return r;

    const Vec3 delta = source - listener;
    const float dist = delta.length();
    if (dist > sp::g_cfg.maxDistance) {
        r.volumeMul = 0.f;
        return r;
    }

    // Distance attenuation (simple inverse, Java uses more complex — keep mobile-friendly)
    float distGain = 1.f / std::max(1.f, dist * sp::g_cfg.attenuationFactor);
    distGain = clampf(distGain, 0.f, 1.f);

    // --- Java SPR core ---
    // occlusionAccumulation = calculateOcclusion(...)
    // directCutoff = exp(-occlusionAccumulation * absorptionCoeff)
    // directGain   = pow(directCutoff, 0.1)
    float occAcc = 0.f;
    if (sp::g_cfg.occlusionEnabled) {
        occAcc = calculateOcclusion(source, listener);
    }
    const float absorption = std::max(0.01f, sp::g_cfg.globalBlockAbsorption);
    float directCutoff = std::exp(-occAcc * absorption);
    float directGain = std::pow(directCutoff, 0.1f);

    // Map to volume / lowpass (SoundPhysicz + Java)
    float occ01 = clampf(occAcc / std::max(1.f, sp::g_cfg.maxOcclusion), 0.f, 1.f);
    // Also apply occlusionVolumeCut on top of directGain
    float vol = directGain * (1.f - occ01 * sp::g_cfg.occlusionVolumeCut * 0.5f);
    vol = clampf(vol, 0.02f, 1.f);

    float lp = sp::g_cfg.maxLowpassHz;
    if (sp::g_cfg.occlusionLowpass) {
        // lower cutoff when more occlusion
        lp = lerpf(sp::g_cfg.maxLowpassHz, sp::g_cfg.minLowpassHz, 1.f - directCutoff);
        lp = clampf(lp, sp::g_cfg.minLowpassHz, sp::g_cfg.maxLowpassHz);
    }

    float rev = evaluateReverbSend(source);
    if (sp::g_cfg.echoEnabled && rev > 0.15f) {
        rev = clampf(rev + rev * sp::g_cfg.echoDecay * 0.35f, 0.f, 1.f);
    }

    // Smooth
    const float sm = clampf(sp::g_cfg.occlusionSmoothing, 0.05f, 1.f);
    Smooth& s = [&]() -> Smooth& {
        std::lock_guard<std::mutex> lock(g_mtx);
        return g_smooth[keyOf(source)];
    }();
    s.occ += (occ01 - s.occ) * sm;
    s.vol += (vol - s.vol) * sm;
    s.lp += (lp - s.lp) * sm;
    s.rev += (rev - s.rev) * sm;

    r.occlusion = s.occ;
    r.volumeMul = clampf(distGain * s.vol, 0.f, 1.f);
    r.lowpassHz = s.lp;
    r.reverbSend = s.rev;

    // Doppler SoS 343 (same idea as SoundPhysicz)
    if (sp::g_cfg.dopplerEnabled && dist > 0.5f) {
        const Vec3 los = delta.normalized();
        const Vec3 relV = sourceVel - listenerVel;
        const float vRadial = relV.x * los.x + relV.y * los.y + relV.z * los.z;
        const float c = std::max(50.f, sp::g_cfg.speedOfSound);
        float pitch = c / (c + vRadial * sp::g_cfg.dopplerScale);
        r.pitchMul = clampf(pitch, 0.85f, 1.15f);
    }
    return r;
}

void tick() {}

} // namespace sp::occ
