#include "reverb/Reverb.hpp"
#include "mc/Raycast.hpp"
#include "core/Types.hpp"

#include <algorithm>
#include <cmath>

namespace sp {
extern Config g_cfg;
}

namespace sp::reverb {
namespace {

struct Room {
    float meanDist{8.f};
    float openness{1.f}; // 1 = outdoor
    float reflectivity{0.2f};
};

Room analyze(const Vec3& listener) {
    Room room{};
    static const sp::Vec3 dirs[] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
        {0.707f, 0, 0.707f}, {-0.707f, 0, 0.707f}, {0.707f, 0, -0.707f}, {-0.707f, 0, -0.707f},
    };
    float sum = 0.f, refl = 0.f;
    int hits = 0;
    const int n = 10;
    for (int i = 0; i < n; ++i) {
        bool hit = false;
        float d = sp::mc::rayDistance(listener, dirs[i], 32.f, hit);
        sum += d;
        if (hit) {
            ++hits;
            auto h = sp::mc::rayCost(listener, listener + dirs[i] * d, 32.f);
            refl += h.reflectivity;
        }
    }
    room.meanDist = sum / float(n);
    room.openness = 1.f - float(hits) / float(n);
    room.reflectivity = hits ? (refl / float(hits)) : 0.1f;
    return room;
}

// FMOD_REVERB_PROPERTIES-like fields we care about (subset)
struct ReverbProps {
    float decayTime;
    float earlyDelay;
    float lateDelay;
    float hfReference;
    float hfDecayRatio;
    float diffusion;
    float density;
    float lowShelfGain;
    float wetLevel; // dB
};

ReverbProps fromRoom(const Room& r) {
    ReverbProps p{};
    // Smaller meanDist => shorter, denser room
    const float size = std::clamp(r.meanDist, 2.f, 40.f);
    p.decayTime = 0.4f + size * 0.08f + r.reflectivity * 1.2f;
    p.earlyDelay = 7.f + size * 0.5f;
    p.lateDelay = 11.f + size * 0.8f;
    p.hfReference = 5000.f;
    p.hfDecayRatio = 0.5f + r.reflectivity * 0.4f;
    p.diffusion = 30.f + r.reflectivity * 70.f;
    p.density = 80.f + (1.f - r.openness) * 20.f;
    p.lowShelfGain = 0.f;
    // outdoor -> very dry
    float wetLin = (1.f - r.openness) * sp::g_cfg.reverbSend;
    wetLin = std::clamp(wetLin, 0.f, 1.f);
    // convert toward dB (0 lin -> -80, 1 -> reverbMaxWetDb)
    const float maxDb = sp::g_cfg.reverbMaxWetDb; // e.g. -6
    p.wetLevel = -80.f + wetLin * (80.f + maxDb);
    return p;
}

} // namespace

// Filled by fmod resolve
using SetReverbPropsFn = int (*)(void* system, int instance, const void* props);
using SetChannelReverbFn = int (*)(void* channel, int instance, float wet);
SetReverbPropsFn g_setReverbProps = nullptr;
SetChannelReverbFn g_setChannelReverb = nullptr;
void* g_fmodSystem = nullptr;

void setFmod(void* system, SetReverbPropsFn a, SetChannelReverbFn b) {
    g_fmodSystem = system;
    g_setReverbProps = a;
    g_setChannelReverb = b;
}

void update(const Vec3& listener) {
    if (!sp::g_cfg.reverbEnabled || !g_fmodSystem || !g_setReverbProps) return;
    Room room = analyze(listener);
    ReverbProps p = fromRoom(room);
    // FMOD expects FMOD_REVERB_PROPERTIES — layout may differ by FMOD version.
    // We pass our subset as a padded buffer; real binding maps fields correctly
    // after confirming FMOD version in libfmod.so / libminecraftpe.
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
    g_setChannelReverb(channel, 0, std::clamp(send, 0.f, 1.f));
}

void shutdown() {
    g_fmodSystem = nullptr;
}

} // namespace sp::reverb

// expose setter for hooks
namespace sp::reverb {
void setFmod(void* system, SetReverbPropsFn a, SetChannelReverbFn b);
}
