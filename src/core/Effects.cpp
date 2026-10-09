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

// Asymptotic 0→1, no hard distance cutoff (real-time room curve)
float continuousT(float dist, float halfDist) {
    if (halfDist < 0.1f) halfDist = 0.1f;
    if (dist < 0.f) dist = 0.f;
    return 1.f - (1.f / (1.f + dist / halfDist));
}

constexpr int kCache = 80;
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

void applySpeedDelay(const fmodapi::Api& api, void* channel, float dist, const Config& cfg) {
    if (!cfg.speedDelayEnabled || !api.setDelay || !api.getDSPClock) return;
    if (dist < 1.f || wasDelayed(channel)) return;

    float travel = dist / cfg.speedOfSound;
    travel = clampf(travel, 0.f, cfg.maxTravelDelaySec);
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

void applyRoomReverb(const fmodapi::Api& api, void* channel, float dist, const Config& cfg) {
    if (!cfg.roomReverbEnabled || !api.setReverb) return;

    const float t = continuousT(dist, cfg.roomHalfDist);
    float wet = lerpf(cfg.roomNear, cfg.roomWetMax, t) * cfg.roomWetScale;
    wet = clampf(wet, 0.f, 1.f);

    api.setReverb(channel, 0, wet);
    api.setReverb(channel, 1, clampf(wet * cfg.roomEarlyRatio, 0.f, 1.f));
    api.setReverb(channel, 2, clampf(wet * cfg.roomLateRatio + 0.06f * t, 0.f, 1.f));
}

void applyEcho(const fmodapi::Api& api, void* channel, float dist, float travelSec, const Config& cfg) {
    if (!cfg.echoEnabled || !api.setReverb) return;
    if (dist < cfg.echoMinDist) return;

    const float t = continuousT(dist - cfg.echoMinDist, cfg.echoHalfDist);
    const float boost = clampf(travelSec * 3.5f, 0.f, 0.45f);
    const float wet = clampf((0.22f + 0.65f * t + boost) * cfg.echoStrength, 0.f, 1.f);

    api.setReverb(channel, 3, wet);
    api.setReverb(channel, 4, wet * 0.55f);
}

void applySoundOcclusion(const fmodapi::Api& api, void* channel, float t, const Config& cfg) {
    if (!cfg.soundOcclusionEnabled || !api.setOcclusion) return;
    api.setOcclusion(channel,
                     lerpf(cfg.occDirectNear, cfg.occDirectFar, t),
                     lerpf(cfg.occReverbNear, cfg.occReverbFar, t));
}

void applyLowOcclusion(const fmodapi::Api& api, void* channel, float t, const Config& cfg) {
    if (!cfg.lowOcclusionEnabled || !api.setLPGain) return;
    api.setLPGain(channel, lerpf(cfg.lowpassNear, cfg.lowpassFar, smooth01(t)));
}

float applyVolumeOcclusion(float volume, float t, const Config& cfg) {
    if (!cfg.volumeOcclusionEnabled) return volume;
    return volume * clampf(lerpf(cfg.volOccNear, cfg.volOccFar, smooth01(t)), 0.f, 1.5f);
}

} // namespace

float apply(const fmodapi::Api& api, void* channel, float volume, const Config& cfg) {
    if (!channel) return volume;

    float dist = fmodapi::distanceListener(api, channel);
    if (dist < 0.f) dist = 0.f;

    const float travel = (cfg.speedOfSound > 1.f) ? (dist / cfg.speedOfSound) : 0.f;
    const float tOcc = continuousT(dist, cfg.refDist * 0.55f);

    applySpeedDelay(api, channel, dist, cfg);
    applySoundOcclusion(api, channel, tOcc, cfg);
    applyLowOcclusion(api, channel, tOcc, cfg);
    applyRoomReverb(api, channel, dist, cfg);
    applyEcho(api, channel, dist, travel, cfg);

    const float outVol = applyVolumeOcclusion(volume, tOcc, cfg);

    static std::atomic<int> n{0};
    if (++n <= 16) {
        SPL_LOGI("v022 FX #%d dist=%.1f roomT=%.2f vol=%.2f->%.2f", n.load(), dist,
                 continuousT(dist, cfg.roomHalfDist), volume, outVol);
    }
    return outVol;
}

} // namespace spl::effects
