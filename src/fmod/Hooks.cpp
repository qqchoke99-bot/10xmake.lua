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
#include <cstring>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "SoundPhysics", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "SoundPhysics", __VA_ARGS__)

// Match FMOD_VECTOR (do not depend on external FMOD headers)
struct FmodVector {
    float x;
    float y;
    float z;
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

using PlaySoundFn = int (*)(void* sys, void* sound, void* group, bool paused, void** channel);
using Set3DAttrFn = int (*)(void* channel, const FmodVector* pos, const FmodVector* vel);
using SetVolumeFn = int (*)(void* channel, float vol);
using SetPitchFn = int (*)(void* channel, float pitch);
using CreateDspFn = int (*)(void* sys, int type, void** dsp);
using AddDspFn = int (*)(void* channel, int index, void* dsp);
using DspSetFloatFn = int (*)(void* dsp, int index, float value);
using Set3DOccFn = int (*)(void* channel, float direct, float reverb);
using SetReverbSysFn = int (*)(void* sys, int inst, const void* props);
using SetReverbChFn = int (*)(void* channel, int inst, float wet);
using SetListenerFn = int (*)(void* sys, int id, const FmodVector* pos, const FmodVector* vel,
                              const FmodVector* forward, const FmodVector* up);

PlaySoundFn orig_play = nullptr;
Set3DAttrFn orig_set3d = nullptr;
SetVolumeFn orig_setVol = nullptr;
SetPitchFn orig_setPitch = nullptr;
SetListenerFn orig_listener = nullptr;
CreateDspFn p_createDsp = nullptr;
AddDspFn p_addDsp = nullptr;
DspSetFloatFn p_dspFloat = nullptr;
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
    void* lowpassDsp = nullptr;
    bool hasPos = false;
};

std::mutex g_mtx;
std::unordered_map<void*, ChanState> g_chan;

void* findSym(void* lib, const char* name) {
    return lib ? dlsym(lib, name) : nullptr;
}

void applyAcoustic(void* channel, ChanState& st) {
    if (!sp::g_cfg.enabled || !st.hasPos) return;

    Vec3 lpos{};
    Vec3 lvel{};
    sp::mc::getListener(lpos, lvel);

    const AcousticResult ac = sp::occ::evaluate(lpos, st.pos, lvel, st.vel);
    // Never store 0 — Minecraft already attenuated; we only scale mildly
    st.lastOccVol = clampf(ac.volumeMul, 0.2f, 1.f);
    st.lastPitch = clampf(ac.pitchMul, 0.85f, 1.15f);

    // set3DOcclusion can mute hard — only when world ray is live and occ > 0
    if (p_set3dOcc && sp::mc::worldReady() && ac.occlusion > 0.05f) {
        const float directOcc = clampf(ac.occlusion * 0.5f, 0.f, 0.7f);
        const float reverbOcc = clampf(1.f - ac.reverbSend, 0.f, 0.7f);
        p_set3dOcc(channel, directOcc, reverbOcc);
    }

    if (sp::g_cfg.occlusionLowpass && ac.occlusion > 0.05f && p_createDsp && p_addDsp &&
        p_dspFloat && g_system) {
        if (!st.lowpassDsp) {
            void* dsp = nullptr;
            if (p_createDsp(g_system, 4, &dsp) == 0 && dsp) {
                p_addDsp(channel, 0, dsp);
                st.lowpassDsp = dsp;
            }
        }
        if (st.lowpassDsp) {
            p_dspFloat(st.lowpassDsp, 0, ac.lowpassHz);
        }
    }

    if (ac.reverbSend > 0.01f) {
        sp::reverb::onChannel(channel, ac.reverbSend);
    }
}

int hk_playSound(void* sys, void* sound, void* group, bool paused, void** channel) {
    g_system = sys;
    const int rc = orig_play ? orig_play(sys, sound, group, paused, channel) : 0;
    if (rc == 0 && channel && *channel) {
        std::lock_guard<std::mutex> lock(g_mtx);
        g_chan[*channel] = ChanState{};

        static bool revBound = false;
        if (!revBound && sys) {
            void* lib = dlopen("libfmod.so", RTLD_NOW | RTLD_NOLOAD);
            if (!lib) lib = dlopen("libminecraftpe.so", RTLD_NOW | RTLD_NOLOAD);
            auto setSys = reinterpret_cast<SetReverbSysFn>(
                findSym(lib, "_ZN4FMOD6System19setReverbPropertiesEiPK22FMOD_REVERB_PROPERTIES"));
            auto setCh = reinterpret_cast<SetReverbChFn>(
                findSym(lib, "_ZN4FMOD14ChannelControl19setReverbPropertiesEif"));
            sp::reverb::setFmod(sys, setSys, setCh);
            revBound = true;
            LOGI("FMOD System captured %p", sys);
        }
    }
    return rc;
}

int hk_set3D(void* channel, const FmodVector* pos, const FmodVector* vel) {
    if (channel && pos) {
        std::lock_guard<std::mutex> lock(g_mtx);
        ChanState& st = g_chan[channel];
        st.pos = Vec3{pos->x, pos->y, pos->z};
        if (vel) st.vel = Vec3{vel->x, vel->y, vel->z};
        st.hasPos = true;
        applyAcoustic(channel, st);
    }
    return orig_set3d ? orig_set3d(channel, pos, vel) : 0;
}

int hk_setVolume(void* channel, float volume) {
    float out = volume;
    if (channel && sp::g_cfg.enabled) {
        std::lock_guard<std::mutex> lock(g_mtx);
        auto it = g_chan.find(channel);
        if (it != g_chan.end() && it->second.hasPos) {
            it->second.baseVol = volume;
            // Only scale when we actually computed a factor; floor 0.2
            const float mul = clampf(it->second.lastOccVol, 0.2f, 1.f);
            out = volume * mul;
        }
    }
    return orig_setVol ? orig_setVol(channel, out) : 0;
}

int hk_setPitch(void* channel, float pitch) {
    float out = pitch;
    if (channel && sp::g_cfg.enabled && sp::g_cfg.dopplerEnabled) {
        std::lock_guard<std::mutex> lock(g_mtx);
        auto it = g_chan.find(channel);
        if (it != g_chan.end()) {
            out = pitch * it->second.lastPitch;
        }
    }
    return orig_setPitch ? orig_setPitch(channel, out) : 0;
}

int hk_listener(void* sys, int id, const FmodVector* pos, const FmodVector* vel,
                const FmodVector* forward, const FmodVector* up) {
    (void)forward;
    (void)up;
    if (pos) {
        const Vec3 p{pos->x, pos->y, pos->z};
        Vec3 v{};
        if (vel) v = Vec3{vel->x, vel->y, vel->z};
        sp::mc::setListener(p, v);
    }
    return orig_listener ? orig_listener(sys, id, pos, vel, forward, up) : 0;
}

} // namespace

bool resolveAndHook() {
    void* lib = dlopen("libfmod.so", RTLD_NOW | RTLD_NOLOAD);
    if (!lib) lib = dlopen("libminecraftpe.so", RTLD_NOW | RTLD_NOLOAD);
    if (!lib) {
        LOGE("dlopen FMOD/minecraftpe failed");
        return false;
    }

    t_play = findSym(lib, "_ZN4FMOD6System9playSoundEPNS_5SoundEPNS_12ChannelGroupEbPPNS_7ChannelE");
    t_3d = findSym(lib, "_ZN4FMOD14ChannelControl15set3DAttributesEPK11FMOD_VECTORS3_");
    t_vol = findSym(lib, "_ZN4FMOD14ChannelControl9setVolumeEf");
    t_pit = findSym(lib, "_ZN4FMOD14ChannelControl8setPitchEf");
    t_list = findSym(lib, "_ZN4FMOD6System24set3DListenerAttributesEiPK11FMOD_VECTORS3_S4_S4_");

    p_createDsp = reinterpret_cast<CreateDspFn>(
        findSym(lib, "_ZN4FMOD6System9createDSPENS_8DSP_TYPEEPPNS_3DSPEb"));
    p_addDsp = reinterpret_cast<AddDspFn>(
        findSym(lib, "_ZN4FMOD14ChannelControl6addDSPEPNS_3DSPEi"));
    p_dspFloat = reinterpret_cast<DspSetFloatFn>(
        findSym(lib, "_ZN4FMOD3DSP17setParameterFloatEif"));
    p_set3dOcc = reinterpret_cast<Set3DOccFn>(
        findSym(lib, "_ZN4FMOD14ChannelControl14set3DOcclusionEff"));

    int ok = 0;
    if (t_play && pl::memory::hook(t_play, reinterpret_cast<void*>(&hk_playSound),
                                   reinterpret_cast<void**>(&orig_play), pl::memory::HookPriority::Normal)) {
        ++ok;
    } else {
        LOGE("playSound hook FAILED");
    }
    if (t_3d && pl::memory::hook(t_3d, reinterpret_cast<void*>(&hk_set3D),
                                 reinterpret_cast<void**>(&orig_set3d), pl::memory::HookPriority::Normal)) {
        ++ok;
    } else {
        LOGE("set3DAttributes hook FAILED");
    }
    if (t_vol && pl::memory::hook(t_vol, reinterpret_cast<void*>(&hk_setVolume),
                                  reinterpret_cast<void**>(&orig_setVol), pl::memory::HookPriority::Normal)) {
        ++ok;
    } else {
        LOGE("setVolume hook FAILED");
    }
    if (t_pit && pl::memory::hook(t_pit, reinterpret_cast<void*>(&hk_setPitch),
                                  reinterpret_cast<void**>(&orig_setPitch), pl::memory::HookPriority::Normal)) {
        ++ok;
    } else {
        LOGE("setPitch hook FAILED");
    }
    if (t_list && pl::memory::hook(t_list, reinterpret_cast<void*>(&hk_listener),
                                   reinterpret_cast<void**>(&orig_listener), pl::memory::HookPriority::Normal)) {
        ++ok;
    } else {
        LOGE("set3DListenerAttributes hook FAILED");
    }

    LOGI("FMOD hooks ok=%d/5 set3DOcclusion=%p", ok, reinterpret_cast<void*>(p_set3dOcc));
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
