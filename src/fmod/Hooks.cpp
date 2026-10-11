#include "fmod/Hooks.hpp"
#include "core/Types.hpp"
#include "mc/Raycast.hpp"
#include "occ/Occlusion.hpp"
#include "reverb/Reverb.hpp"

#include <android/log.h>
#include <dlfcn.h>
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <cmath>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "SoundPhysics", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "SoundPhysics", __VA_ARGS__)

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
using SetPitchFn  = int (*)(void* channel, float pitch);
using CreateDspFn = int (*)(void* sys, int type, void** dsp);
using AddDspFn    = int (*)(void* channel, int index, void* dsp);
using DspSetFloatFn = int (*)(void* dsp, int index, float value);
using Set3DOccFn  = int (*)(void* channel, float direct, float reverb);
using SetReverbSysFn = int (*)(void* sys, int inst, const void* props);
using SetReverbChFn  = int (*)(void* channel, int inst, float wet);
using SetListenerFn = int (*)(void* sys, int id, const FmodVector* pos, const FmodVector* vel,
                              const FmodVector* forward, const FmodVector* up);

PlaySoundFn  orig_play = nullptr;
Set3DAttrFn  orig_set3d = nullptr;
SetVolumeFn  orig_setVol = nullptr;
SetPitchFn   orig_setPitch = nullptr;
SetListenerFn orig_listener = nullptr;
CreateDspFn  p_createDsp = nullptr;
AddDspFn     p_addDsp = nullptr;
DspSetFloatFn p_dspFloat = nullptr;
Set3DOccFn   p_set3dOcc = nullptr;

void* g_system = nullptr;
void* t_play = nullptr;
void* t_3d = nullptr;
void* t_vol = nullptr;
void* t_pit = nullptr;
void* t_list = nullptr;

struct ChanState {
    Vec3 pos{};
    Vec3 vel{};
    float baseVol{1.f};
    float lastOccVol{1.f};
    float lastPitch{1.f};
    void* lowpassDsp{nullptr};
    bool hasPos{false};
};

std::mutex g_mtx;
std::unordered_map<void*, ChanState> g_chan;

void* findSym(void* lib, const char* name) { return dlsym(lib, name); }

void applyAcoustic(void* channel, ChanState& st) {
    if (!sp::g_cfg.enabled || !st.hasPos) return;
    Vec3 lpos{}, lvel{};
    sp::mc::getListener(lpos, lvel);
    auto ac = sp::occ::evaluate(lpos, st.pos, lvel, st.vel);
    st.lastOccVol = ac.volumeMul;
    st.lastPitch = ac.pitchMul;

    if (p_set3dOcc) {
        p_set3dOcc(channel,
                   std::clamp(1.f - ac.volumeMul, 0.f, 1.f),
                   std::clamp(1.f - ac.reverbSend, 0.f, 1.f));
    }
    if (sp::g_cfg.occlusionLowpass && p_createDsp && p_addDsp && p_dspFloat && g_system) {
        if (!st.lowpassDsp) {
            void* dsp = nullptr;
            if (p_createDsp(g_system, 4, &dsp) == 0 && dsp) {
                p_addDsp(channel, 0, dsp);
                st.lowpassDsp = dsp;
            }
        }
        if (st.lowpassDsp) p_dspFloat(st.lowpassDsp, 0, ac.lowpassHz);
    }
    sp::reverb::onChannel(channel, ac.reverbSend);
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
            auto setSys = (SetReverbSysFn)(lib ? findSym(lib, "_ZN4FMOD6System19setReverbPropertiesEiPK22FMOD_REVERB_PROPERTIES") : nullptr);
            auto setCh  = (SetReverbChFn)(lib ? findSym(lib, "_ZN4FMOD14ChannelControl19setReverbPropertiesEif") : nullptr);
            sp::reverb::setFmod(sys, setSys, setCh);
            revBound = true;
            LOGI("FMOD::System* captured %p reverb=%p", sys, (void*)setSys);
        }
    }
    return rc;
}

int hk_set3d(void* channel, const FmodVector* pos, const FmodVector* vel) {
    if (channel && pos) {
        std::lock_guard<std::mutex> lock(g_mtx);
        auto& st = g_chan[channel];
        st.pos = {pos->x, pos->y, pos->z};
        if (vel) st.vel = {vel->x, vel->y, vel->z};
        st.hasPos = true;
    }
    return orig_set3d ? orig_set3d(channel, pos, vel) : 0;
}

int hk_setVolume(void* channel, float vol) {
    float out = vol;
    if (channel && sp::g_cfg.enabled) {
        std::lock_guard<std::mutex> lock(g_mtx);
        auto it = g_chan.find(channel);
        if (it != g_chan.end()) {
            it->second.baseVol = vol;
            applyAcoustic(channel, it->second);
            out = vol * it->second.lastOccVol;
        }
    }
    return orig_setVol ? orig_setVol(channel, out) : 0;
}

int hk_setPitch(void* channel, float pitch) {
    float out = pitch;
    if (channel && sp::g_cfg.enabled && sp::g_cfg.dopplerEnabled) {
        std::lock_guard<std::mutex> lock(g_mtx);
        auto it = g_chan.find(channel);
        if (it != g_chan.end()) out = pitch * it->second.lastPitch;
    }
    return orig_setPitch ? orig_setPitch(channel, out) : 0;
}

int hk_listener(void* sys, int id, const FmodVector* pos, const FmodVector* vel,
                const FmodVector* forward, const FmodVector* up) {
    if (id == 0 && pos) {
        Vec3 p{pos->x, pos->y, pos->z};
        Vec3 v = vel ? Vec3{vel->x, vel->y, vel->z} : Vec3{};
        sp::mc::setListener(p, v);
        static int tick = 0;
        if ((++tick % 30) == 0) sp::reverb::update(p);
    }
    return orig_listener ? orig_listener(sys, id, pos, vel, forward, up) : 0;
}

bool hookOne(void* target, void* detour, void** orig) {
    if (!target || !detour) return false;
    return pl::memory::hook(target, detour, orig, pl::memory::HookPriority::Normal);
}

} // namespace

bool resolveAndHook() {
    void* lib = dlopen("libfmod.so", RTLD_NOW | RTLD_NOLOAD);
    if (!lib) lib = dlopen("libminecraftpe.so", RTLD_NOW | RTLD_NOLOAD);
    if (!lib) {
        LOGE("dlopen failed");
        return false;
    }

    t_play = findSym(lib, "_ZN4FMOD6System9playSoundEPNS_5SoundEPNS_12ChannelGroupEbPPNS_7ChannelE");
    t_3d   = findSym(lib, "_ZN4FMOD14ChannelControl15set3DAttributesEPK11FMOD_VECTORS3_");
    t_vol  = findSym(lib, "_ZN4FMOD14ChannelControl9setVolumeEf");
    t_pit  = findSym(lib, "_ZN4FMOD14ChannelControl8setPitchEf");
    t_list = findSym(lib, "_ZN4FMOD6System22set3DListenerAttributesEiPK11FMOD_VECTORS3_S4_S4_");
    p_createDsp = (CreateDspFn)findSym(lib, "_ZN4FMOD6System15createDSPByTypeE13FMOD_DSP_TYPEPPNS_3DSPE");
    p_addDsp    = (AddDspFn)findSym(lib, "_ZN4FMOD14ChannelControl6addDSPEiPNS_3DSPE");
    p_dspFloat  = (DspSetFloatFn)findSym(lib, "_ZN4FMOD3DSP17setParameterFloatEif");
    p_set3dOcc  = (Set3DOccFn)findSym(lib, "_ZN4FMOD14ChannelControl14set3DOcclusionEff");

    bool ok = true;
    if (t_play) ok &= hookOne(t_play, (void*)hk_playSound, (void**)&orig_play); else LOGE("playSound missing");
    if (t_3d)   ok &= hookOne(t_3d,   (void*)hk_set3d,     (void**)&orig_set3d); else LOGE("set3DAttr missing");
    if (t_vol)  ok &= hookOne(t_vol,  (void*)hk_setVolume, (void**)&orig_setVol); else LOGE("setVolume missing");
    if (t_pit)  hookOne(t_pit, (void*)hk_setPitch, (void**)&orig_setPitch);
    if (t_list) hookOne(t_list, (void*)hk_listener, (void**)&orig_listener);

    LOGI("FMOD hooks ok=%d dsp=%p occ=%p listener=%p", ok, (void*)p_createDsp, (void*)p_set3dOcc, t_list);
    return ok;
}

void unhookAll() {
    if (t_play && orig_play) pl::memory::unhook(t_play, (void*)hk_playSound);
    if (t_3d && orig_set3d) pl::memory::unhook(t_3d, (void*)hk_set3d);
    if (t_vol && orig_setVol) pl::memory::unhook(t_vol, (void*)hk_setVolume);
    if (t_pit && orig_setPitch) pl::memory::unhook(t_pit, (void*)hk_setPitch);
    if (t_list && orig_listener) pl::memory::unhook(t_list, (void*)hk_listener);
    sp::reverb::shutdown();
    std::lock_guard<std::mutex> lock(g_mtx);
    g_chan.clear();
}

} // namespace sp::fmodx
