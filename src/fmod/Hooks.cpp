#include "fmod/Hooks.hpp"
#include "core/Types.hpp"
#include "mc/Raycast.hpp"
#include "occ/Occlusion.hpp"
#include "reverb/Reverb.hpp"

#include <android/log.h>
#include <dlfcn.h>
#include <mutex>
#include <unordered_map>
#include <cmath>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "SoundPhysics", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "SoundPhysics", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "SoundPhysics", __VA_ARGS__)

struct FmodVector {
    float x, y, z;
};

namespace pl::memory {
enum class HookPriority : int { Lowest = 0, Low = 100, Normal = 200, High = 300, Highest = 400 };
bool hook(void* target, void* detour, void** original, HookPriority priority);
bool unhook(void* target, void* detour);
}

namespace sp {
extern Config g_cfg;
}

namespace sp::fmodx {
namespace {

// NOTE: FMOD Channel inherits ChannelControl; mangled names use ChannelControl.
using PlaySoundFn = int (*)(void* sys, void* sound, void* group, bool paused, void** channel);
using Set3DAttrFn = int (*)(void* channel, const FmodVector* pos, const FmodVector* vel);
using SetVolumeFn = int (*)(void* channel, float vol);
using SetPitchFn = int (*)(void* channel, float pitch);
using GetModeFn = int (*)(void* channel, unsigned int* mode);
using Set3DOccFn = int (*)(void* channel, float direct, float reverb);
using SetListenerFn = int (*)(void* sys, int id, const FmodVector* pos, const FmodVector* vel,
                              const FmodVector* forward, const FmodVector* up);

PlaySoundFn orig_play = nullptr;
Set3DAttrFn orig_set3d = nullptr;
SetVolumeFn orig_setVol = nullptr;
SetPitchFn orig_setPitch = nullptr;
SetListenerFn orig_listener = nullptr;
GetModeFn p_getMode = nullptr;
Set3DOccFn p_set3dOcc = nullptr;

void* g_system = nullptr;
void* t_play = nullptr;
void* t_3d = nullptr;
void* t_vol = nullptr;
void* t_pit = nullptr;
void* t_list = nullptr;

struct ChanState {
    Vec3 pos{};
    Vec3 vel{};
    float baseVol = 1.f;
    float lastOccVol = 1.f;
    float lastPitch = 1.f;
    bool hasPos = false;
    bool is3d = false;
};

std::mutex g_mtx;
std::unordered_map<void*, ChanState> g_chan;

// Stats for heartbeat
int g_playN = 0, g_attrN = 0, g_volN = 0;

void* findSym(void* lib, const char* name) { return lib ? dlsym(lib, name) : nullptr; }

bool effectsActive() {
    // Master off → pure pass-through, zero side effects
    return sp::g_cfg.enabled;
}

void applyAcoustic(void* channel, ChanState& st) {
    if (!effectsActive() || !st.hasPos) return;

    Vec3 lpos{}, lvel{};
    sp::mc::getListener(lpos, lvel);

    // If listener never updated, do nothing (prevents bad distance math)
    if (std::fabs(lpos.x) + std::fabs(lpos.y) + std::fabs(lpos.z) < 0.01f) return;

    const AcousticResult ac = sp::occ::evaluate(lpos, st.pos, lvel, st.vel);
    st.lastOccVol = clampf(ac.volumeMul, 0.25f, 1.f);
    st.lastPitch = clampf(ac.pitchMul, 0.9f, 1.1f);

    // Prefer FMOD native occlusion over volume multiply (less likely to mute dry path)
    if (p_set3dOcc && sp::mc::worldReady() && ac.occlusion > 0.08f) {
        p_set3dOcc(channel, clampf(ac.occlusion * 0.45f, 0.f, 0.65f),
                   clampf(1.f - ac.reverbSend, 0.f, 0.5f));
    }

    // Soft reverb send only — never system-wide wet=-80
    if (ac.reverbSend > 0.05f) {
        sp::reverb::onChannel(channel, ac.reverbSend);
    }
}

int hk_playSound(void* sys, void* sound, void* group, bool paused, void** channel) {
    g_system = sys;
    const int rc = orig_play ? orig_play(sys, sound, group, paused, channel) : -1;
    if (rc == 0 && channel && *channel) {
        std::lock_guard<std::mutex> lock(g_mtx);
        ChanState st{};
        if (p_getMode) {
            unsigned int mode = 0;
            p_getMode(*channel, &mode);
            st.is3d = (mode & 0x00000010) != 0; // FMOD_3D
            LOGI("new channel mode=0x%x is3D=%d", mode, st.is3d ? 1 : 0);
        } else {
            LOGI("new channel");
        }
        g_chan[*channel] = st;
        ++g_playN;
    }
    return rc;
}

int hk_set3D(void* channel, const FmodVector* pos, const FmodVector* vel) {
    // Always call original first so position is correct for FMOD engine
    const int rc = orig_set3d ? orig_set3d(channel, pos, vel) : 0;
    ++g_attrN;
    if (channel && pos && effectsActive()) {
        std::lock_guard<std::mutex> lock(g_mtx);
        auto& st = g_chan[channel];
        st.pos = Vec3{pos->x, pos->y, pos->z};
        if (vel) st.vel = Vec3{vel->x, vel->y, vel->z};
        st.hasPos = true;
        st.is3d = true;
        applyAcoustic(channel, st);
    }
    return rc;
}

int hk_setVolume(void* channel, float volume) {
    // CRITICAL: when disabled → pure pass-through (this was silencing everything)
    if (!effectsActive() || !channel) {
        return orig_setVol ? orig_setVol(channel, volume) : 0;
    }

    float out = volume;
    {
        std::lock_guard<std::mutex> lock(g_mtx);
        auto it = g_chan.find(channel);
        if (it != g_chan.end() && it->second.hasPos) {
            it->second.baseVol = volume;
            // Only scale if occlusion path actually reduced; floor 0.25
            const float mul = clampf(it->second.lastOccVol, 0.25f, 1.f);
            // If no world occlusion, mul stays ~1 → no change
            out = volume * mul;
        }
    }
    ++g_volN;
    return orig_setVol ? orig_setVol(channel, out) : 0;
}

int hk_setPitch(void* channel, float pitch) {
    if (!effectsActive() || !sp::g_cfg.dopplerEnabled || !channel) {
        return orig_setPitch ? orig_setPitch(channel, pitch) : 0;
    }
    float out = pitch;
    {
        std::lock_guard<std::mutex> lock(g_mtx);
        auto it = g_chan.find(channel);
        if (it != g_chan.end()) out = pitch * clampf(it->second.lastPitch, 0.9f, 1.1f);
    }
    return orig_setPitch ? orig_setPitch(channel, out) : 0;
}

int hk_listener(void* sys, int id, const FmodVector* pos, const FmodVector* vel,
                const FmodVector* forward, const FmodVector* up) {
    if (pos) {
        Vec3 p{pos->x, pos->y, pos->z};
        Vec3 v{};
        if (vel) v = Vec3{vel->x, vel->y, vel->z};
        sp::mc::setListener(p, v);
    }
    return orig_listener ? orig_listener(sys, id, pos, vel, forward, up) : 0;
}

void heartbeat() {
    static int last = 0;
    // Log every ~2s of activity via call counts (caller can also timer)
    const int total = g_playN + g_attrN + g_volN;
    if (total - last < 8) return;
    last = total;
    size_t n = 0, withPos = 0;
    {
        std::lock_guard<std::mutex> lock(g_mtx);
        n = g_chan.size();
        for (auto& kv : g_chan)
            if (kv.second.hasPos) ++withPos;
    }
    LOGI("hb en=%d bs=%p play=%d attr=%d vol=%d chans=%zu withPos=%zu",
         effectsActive() ? 1 : 0, sp::mc::g_blockSource, g_playN, g_attrN, g_volN, n,
         withPos);
    // One-shot getBlock probe near listener (occlusion path)
    if (sp::mc::g_blockSource) {
        static int probe = 0;
        if ((++probe % 30) == 1) {
            Vec3 lp{}, lv{};
            sp::mc::getListener(lp, lv);
            const int x = (int)std::floor(lp.x);
            const int y = (int)std::floor(lp.y);
            const int z = (int)std::floor(lp.z);
            void* air = sp::mc::getBlockAt(x, y + 2, z);   // often air
            void* feet = sp::mc::getBlockAt(x, y - 1, z); // often solid
            float cAir = sp::mc::blockCost(air);
            float cFeet = sp::mc::blockCost(feet);
            LOGI("getBlock probe air=%p cost=%.1f feet=%p cost=%.1f worldReady=%d",
                 air, cAir, feet, cFeet, sp::mc::worldReady() ? 1 : 0);
        }
    }
}

} // namespace

bool resolveAndHook() {
    void* lib = dlopen("libfmod.so", RTLD_NOW | RTLD_NOLOAD);
    if (!lib) lib = dlopen("libminecraftpe.so", RTLD_NOW | RTLD_NOLOAD);
    if (!lib) {
        LOGE("dlopen FMOD/minecraftpe failed");
        return false;
    }

    // Same symbols as libSoundPhysicz.so
    t_play = findSym(lib, "_ZN4FMOD6System9playSoundEPNS_5SoundEPNS_12ChannelGroupEbPPNS_7ChannelE");
    t_3d = findSym(lib, "_ZN4FMOD14ChannelControl15set3DAttributesEPK11FMOD_VECTORS3_");
    t_vol = findSym(lib, "_ZN4FMOD14ChannelControl9setVolumeEf");
    t_pit = findSym(lib, "_ZN4FMOD14ChannelControl8setPitchEf");
    t_list = findSym(lib, "_ZN4FMOD6System24set3DListenerAttributesEiPK11FMOD_VECTORS3_S4_S4_");
    p_getMode = reinterpret_cast<GetModeFn>(
        findSym(lib, "_ZN4FMOD14ChannelControl7getModeEPj"));
    p_set3dOcc = reinterpret_cast<Set3DOccFn>(
        findSym(lib, "_ZN4FMOD14ChannelControl14set3DOcclusionEff"));

    LOGI("sym play=%p 3d=%p vol=%p pit=%p list=%p occ=%p mode=%p", t_play, t_3d, t_vol, t_pit,
         t_list, (void*)p_set3dOcc, (void*)p_getMode);

    int ok = 0;
    auto tryHook = [&](void* tgt, void* det, void** orig, const char* name) {
        if (!tgt) {
            LOGE("%s symbol NULL", name);
            return;
        }
        if (pl::memory::hook(tgt, det, orig, pl::memory::HookPriority::Normal)) {
            ++ok;
            LOGI("%s hook OK", name);
        } else {
            LOGE("%s hook FAILED", name);
        }
    };

    tryHook(t_play, reinterpret_cast<void*>(&hk_playSound), reinterpret_cast<void**>(&orig_play),
            "playSound");
    tryHook(t_3d, reinterpret_cast<void*>(&hk_set3D), reinterpret_cast<void**>(&orig_set3d),
            "set3DAttributes");
    tryHook(t_vol, reinterpret_cast<void*>(&hk_setVolume), reinterpret_cast<void**>(&orig_setVol),
            "setVolume");
    tryHook(t_pit, reinterpret_cast<void*>(&hk_setPitch), reinterpret_cast<void**>(&orig_setPitch),
            "setPitch");
    tryHook(t_list, reinterpret_cast<void*>(&hk_listener), reinterpret_cast<void**>(&orig_listener),
            "set3DListenerAttributes");

    // IMPORTANT: do NOT call System::setReverbProperties with wet=-80 (mutes ambient)
    // Channel-level reverb send only via onChannel when effects on.

    LOGI("FMOD hooks ok=%d/5 set3DOcclusion=%p", ok, (void*)p_set3dOcc);
    return ok > 0;
}

void unhookAll() {
    if (t_play && orig_play) pl::memory::unhook(t_play, reinterpret_cast<void*>(&hk_playSound));
    if (t_3d && orig_set3d) pl::memory::unhook(t_3d, reinterpret_cast<void*>(&hk_set3D));
    if (t_vol && orig_setVol) pl::memory::unhook(t_vol, reinterpret_cast<void*>(&hk_setVolume));
    if (t_pit && orig_setPitch) pl::memory::unhook(t_pit, reinterpret_cast<void*>(&hk_setPitch));
    if (t_list && orig_listener) pl::memory::unhook(t_list, reinterpret_cast<void*>(&hk_listener));
    orig_play = nullptr;
    orig_set3d = nullptr;
    orig_setVol = nullptr;
    orig_setPitch = nullptr;
    orig_listener = nullptr;
    std::lock_guard<std::mutex> lock(g_mtx);
    g_chan.clear();
}

} // namespace sp::fmodx
