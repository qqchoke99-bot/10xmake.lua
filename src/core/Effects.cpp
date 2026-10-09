#include "Effects.hpp"
#include <soundphysics/Log.hpp>
#include <atomic>
#include <cmath>

namespace spl::effects {
namespace {

float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float lerpf(float a, float b, float t) { return a + (b - a) * t; }

float smooth01(float t) {
    t = clampf(t, 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

// SPR-like inverse-square attenuation with attenuationFactor
// vol = 1 / (1 + attenuationFactor * (dist/ref)^2)
float sprAttenuation(float dist, float refDist, float factor) {
    if (refDist < 0.5f) refDist = 0.5f;
    if (factor < 0.05f) factor = 0.05f;
    const float r = dist / refDist;
    return 1.f / (1.f + factor * r * r);
}

// Continuous 0..1 growth without hard cutoff
float continuousT(float dist, float half) {
    if (half < 0.1f) half = 0.1f;
    if (dist < 0.f) dist = 0.f;
    return 1.f - (1.f / (1.f + dist / half));
}

constexpr int kCache = 96;
void* gDelayed[kCache]{};
int gDelayedIdx = 0;

bool wasDelayed(void* ch) {
    for (int i = 0; i < kCache; ++i)
        if (gDelayed[i] == ch) return true;
    return false;
}

void markDelayed(void* ch) {
    gDelayed[gDelayedIdx % kCache] = ch;
    gDelayedIdx = (gDelayedIdx + 1) % kCache;
}

void applySpeedDelay(const fmodapi::Api& api, void* channel, float dist, const cfg::Settings& s) {
    if (!s.speedOfSoundEnabled || !api.setDelay || !api.getDSPClock) return;
    if (dist < 1.f || wasDelayed(channel)) return;

    float travel = dist / s.speedOfSound;
    travel = clampf(travel, 0.f, s.maxTravelDelaySec);
    if (travel < 0.008f) {
        markDelayed(channel);
        return;
    }

    int rate = 48000;
    if (api.getSystem && api.getSoftFmt) {
        void* sys = nullptr;
        if (api.getSystem(channel, &sys) == 0 && sys) {
            int sr = 0, mode = 0, raw = 0;
            if (api.getSoftFmt(sys, &sr, &mode, &raw) == 0 && sr > 0) rate = sr;
        }
    }

    unsigned long long clock = 0, parent = 0;
    if (api.getDSPClock(channel, &clock, &parent) != 0) return;
    const auto samples = static_cast<unsigned long long>(travel * static_cast<float>(rate));
    api.setDelay(channel, clock + samples, 0, 0);
    markDelayed(channel);
}

} // namespace

float apply(const fmodapi::Api& api, void* channel, float volume, const cfg::Settings& s) {
    if (!channel || !s.enabled) return volume;

    float dist = fmodapi::distanceListener(api, channel);
    if (dist < 0.f) dist = 0.f;

    if (dist > s.maxSoundProcessingDistance) {
        // Still attenuate heavily past max processing distance
        if (s.attenuationEnabled) {
            float att = sprAttenuation(dist, s.refDistance, s.attenuationFactor * s.globalIntensity);
            return volume * clampf(att, 0.02f, 1.f);
        }
        return volume;
    }

    const float g = clampf(s.globalIntensity, 0.1f, 3.f);
    const float t = continuousT(dist, s.maxSoundProcessingDistance * 0.35f);
    const float tSmooth = smooth01(t);

    // ---- 1) SPR attenuation (volume vs distance) ----
    float outVol = volume;
    if (s.attenuationEnabled) {
        float att = sprAttenuation(dist, s.refDistance, s.attenuationFactor * g);
        outVol *= clampf(att, 0.02f, 1.f);
    }

    // ---- 2) Occlusion proxy (distance → blocked feel until real rays) ----
    // SPR: occlusion from blocks; we use distance-shaped occlusion * intensity
    float occ = 0.f;
    if (s.soundOcclusionEnabled || s.volumeOcclusionEnabled || s.airAbsorptionEnabled) {
        occ = clampf(tSmooth * s.maxOcclusion * s.occlusionIntensity * g, 0.f, 1.f);
    }

    if (s.soundOcclusionEnabled && api.setOcclusion) {
        // direct path occlusion + reverb path slightly less
        const float direct = occ;
        const float reverbOcc = occ * 0.65f;
        api.setOcclusion(channel, direct, reverbOcc);
    }

    if (s.volumeOcclusionEnabled) {
        // extra volume drop when "occluded"
        const float volMul = 1.f - occ * 0.55f * s.volumeOcclusionIntensity;
        outVol *= clampf(volMul, 0.05f, 1.f);
    }

    // ---- 3) Air absorption / low occlusion (lowpass) — SPR airAbsorption ----
    if (s.airAbsorptionEnabled && api.setLPGain) {
        // higher airAbsorption → more muffling with distance/occlusion
        const float absorb = clampf(s.airAbsorption * g, 0.f, 1.f);
        const float lp = lerpf(1.f, 0.08f, tSmooth * absorb * (0.55f + 0.45f * occ));
        api.setLPGain(channel, clampf(lp, 0.05f, 1.f));
    }

    // ---- 4) Room reverb — SPR reverbGain / reverbDistance / reverbBrightness ----
    if (s.roomReverbEnabled && api.setReverb) {
        const float rd = s.reverbDistance > 0.1f ? s.reverbDistance : 1.f;
        const float rT = continuousT(dist, (s.maxSoundProcessingDistance * 0.28f) / rd);
        float wet = s.reverbGain * g * lerpf(0.05f, 0.95f, rT);
        // brighter = less dark on late slots; darker reduces high (via lower late wet slightly)
        wet *= lerpf(0.75f, 1.1f, clampf(s.reverbBrightness, 0.f, 1.f));
        // less reverb on heavily occluded direct path (energy trapped)
        wet *= (1.f - occ * 0.35f);
        wet = clampf(wet, 0.f, 1.f);

        api.setReverb(channel, 0, wet);
        api.setReverb(channel, 1, clampf(wet * 0.40f, 0.f, 1.f)); // early
        api.setReverb(channel, 2, clampf(wet * 0.65f, 0.f, 1.f)); // late
    }

    // ---- 5) Echo (reflection energy loss SPR-style) ----
    if (s.echoEnabled && api.setReverb && dist > 6.f) {
        const float eT = continuousT(dist - 6.f, 28.f);
        const float travel = dist / (s.speedOfSound > 1.f ? s.speedOfSound : 343.f);
        float echoWet = (0.20f + 0.70f * eT + clampf(travel * 3.f, 0.f, 0.4f)) * s.echoIntensity * g;
        echoWet *= (1.f - occ * 0.25f);
        echoWet = clampf(echoWet, 0.f, 1.f);
        api.setReverb(channel, 3, echoWet);
        api.setReverb(channel, 4, echoWet * 0.5f);
    }

    // ---- 6) Speed of sound delay ----
    applySpeedDelay(api, channel, dist, s);

    static std::atomic<int> n{0};
    if (s.debugLogging && ++n <= 20) {
        SPL_LOGI("SPR-FX #%d dist=%.1f occ=%.2f vol=%.2f->%.2f g=%.2f", n.load(), dist, occ, volume,
                 outVol, g);
    }

    return outVol;
}

} // namespace spl::effects
