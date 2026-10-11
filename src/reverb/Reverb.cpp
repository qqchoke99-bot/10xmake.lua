#include "reverb/Reverb.hpp"
#include "core/Types.hpp"
#include <cmath>

namespace sp {
extern Config g_cfg;
}

namespace sp::reverb {
namespace {

SetReverbPropsFn g_setReverbProps = nullptr;
SetChannelReverbFn g_setChannelReverb = nullptr;
void* g_fmodSystem = nullptr;

} // namespace

void setFmod(void* system, SetReverbPropsFn a, SetChannelReverbFn b) {
    g_fmodSystem = system;
    g_setReverbProps = a;
    g_setChannelReverb = b;
    // Do NOT push wet=-80 system reverb — that can silence the whole mix.
}

void update(const Vec3& /*listener*/) {
    // Disabled: system-wide setReverbProperties was a silence source.
    // Room analysis can return later once channel sends are verified.
    (void)g_fmodSystem;
    (void)g_setReverbProps;
}

void onChannel(void* channel, float send) {
    if (!sp::g_cfg.enabled || !sp::g_cfg.reverbEnabled) return;
    if (!g_setChannelReverb || !channel) return;
    // Mild wet only
    g_setChannelReverb(channel, 0, clampf(send * 0.35f, 0.f, 0.5f));
}

void shutdown() {
    g_fmodSystem = nullptr;
    g_setReverbProps = nullptr;
    g_setChannelReverb = nullptr;
}

} // namespace sp::reverb
