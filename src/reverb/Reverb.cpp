#include "reverb/Reverb.hpp"
#include "mc/Raycast.hpp"
#include "core/Types.hpp"

#include <cmath>

namespace sp {
extern Config g_cfg;
}

namespace sp::reverb {
namespace {

struct Room {
    float meanDist = 8.f;
    float openness = 1.f; // 1 = outdoor
    float reflectivity = 0.2f;
};

Room analyze(const Vec3& listener) {
    Room room{};
    static const Vec3 dirs[] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
        {0.707f, 0, 0.707f}, {-0.707f, 0, 0.707f}, {0.707f, 0, -0.707f}, {-0.707f, 0, -0.707f},
    };
    float sum = 0.f;
    float refl = 0.f;
    int hits = 0;
    constexpr int n = 10;
    constexpr float maxRange = 32.f;

    for (int i = 0; i < n; ++i) {
        const Vec3 end = listener + dirs[i] * maxRange;
        bool hit = false;
        // API: rayDistance(from, to, hitOut)
        const float d = sp::mc::rayDistance(listener, end, hit);
        sum += d;
        if (hit) {
            ++hits;
            // API: rayCost(from, to) -> float cost
            const float cost = sp::mc::rayCost(listener, end);
            // Map cost to a soft reflectivity cue
            refl += clampf(0.3f + cost * 0.05f, 0.1f, 1.f);
        }
    }

    room.meanDist = sum / float(n);
    room.openness = 1.f - float(hits) / float(n);
    room.reflectivity = hits ? (refl / float(hits)) : 0.1f;
    return room;
}

struct ReverbProps {
    float decayTime = 1.f;
    float earlyDelay = 7.f;
    float lateDelay = 11.f;
    float hfReference = 5000.f;
    float hfDecayRatio = 0.5f;
    float diffusion = 50.f;
    float density = 100.f;
    float lowShelfGain = 0.f;
    float wetLevel = -80.f; // dB
};

ReverbProps fromRoom(const Room& r) {
    ReverbProps p{};
    const float size = clampf(r.meanDist, 2.f, 40.f);
    p.decayTime = 0.4f + size * 0.08f + r.reflectivity * 1.2f;
    p.earlyDelay = 7.f + size * 0.5f;
    p.lateDelay = 11.f + size * 0.8f;
    p.hfReference = 5000.f;
    p.hfDecayRatio = 0.5f + r.reflectivity * 0.4f;
    p.diffusion = 30.f + r.reflectivity * 70.f;
    p.density = 80.f + (1.f - r.openness) * 20.f;
    p.lowShelfGain = 0.f;
    float wetLin = (1.f - r.openness) * sp::g_cfg.reverbSend;
    wetLin = clampf(wetLin, 0.f, 1.f);
    const float maxDb = sp::g_cfg.reverbMaxWetDb;
    p.wetLevel = -80.f + wetLin * (80.f + maxDb);
    return p;
}

SetReverbPropsFn g_setReverbProps = nullptr;
SetChannelReverbFn g_setChannelReverb = nullptr;
void* g_fmodSystem = nullptr;

} // namespace

void setFmod(void* system, SetReverbPropsFn a, SetChannelReverbFn b) {
    g_fmodSystem = system;
    g_setReverbProps = a;
    g_setChannelReverb = b;
}

void update(const Vec3& listener) {
    if (!sp::g_cfg.reverbEnabled || !g_fmodSystem || !g_setReverbProps) return;
    const Room room = analyze(listener);
    const ReverbProps p = fromRoom(room);
    alignas(16) float buf[32]{};
    buf[0] = p.decayTime;
    buf[1] = p.earlyDelay;
    buf[2] = p.lateDelay;
    buf[3] = p.hfReference;
    buf[4] = p.hfDecayRatio;
    buf[5] = p.diffusion;
    buf[6] = p.density;
    buf[7] = p.lowShelfGain;
    buf[8] = p.wetLevel;
    g_setReverbProps(g_fmodSystem, 0, buf);
}

void onChannel(void* channel, float send) {
    if (!sp::g_cfg.reverbEnabled || !g_setChannelReverb || !channel) return;
    g_setChannelReverb(channel, 0, clampf(send, 0.f, 1.f));
}

void shutdown() {
    g_fmodSystem = nullptr;
    g_setReverbProps = nullptr;
    g_setChannelReverb = nullptr;
}

} // namespace sp::reverb
