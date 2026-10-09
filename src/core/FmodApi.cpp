#include "FmodApi.hpp"
#include "Hooks.hpp"
#include <soundphysics/Log.hpp>
#include <cmath>
#include <cstring>

namespace spl::fmodapi {
namespace {

void* resolveSym(void* lib, const char* a, const char* b) {
    void* p = spl::hooks::symbol(lib, a);
    return p ? p : (b ? spl::hooks::symbol(lib, b) : nullptr);
}

} // namespace

bool resolve(Api& api) {
    if (!api.lib) {
        api.lib = spl::hooks::openLibrary("libfmod.so");
    }
    if (!api.lib) {
        SPL_LOGW("libfmod.so not found");
        return false;
    }

    api.setLPGain = reinterpret_cast<FN_SetLowPassGain>(
        resolveSym(api.lib, "FMOD_Channel_SetLowPassGain", "FMOD5_Channel_SetLowPassGain"));
    api.setReverb = reinterpret_cast<FN_SetReverbProps>(
        resolveSym(api.lib, "FMOD_Channel_SetReverbProperties", "FMOD5_Channel_SetReverbProperties"));
    api.setOcclusion = reinterpret_cast<FN_Set3DOcclusion>(
        resolveSym(api.lib, "FMOD_Channel_Set3DOcclusion", "FMOD5_Channel_Set3DOcclusion"));
    api.get3DAttr = reinterpret_cast<FN_Get3DAttributes>(
        resolveSym(api.lib, "FMOD_Channel_Get3DAttributes", "FMOD5_Channel_Get3DAttributes"));
    api.getSystem = reinterpret_cast<FN_GetSystemObject>(
        resolveSym(api.lib, "FMOD_Channel_GetSystemObject", "FMOD5_Channel_GetSystemObject"));
    api.getListener = reinterpret_cast<FN_Get3DListenerAttributes>(
        resolveSym(api.lib, "FMOD_System_Get3DListenerAttributes", "FMOD5_System_Get3DListenerAttributes"));
    api.getDSPClock = reinterpret_cast<FN_GetDSPClock>(
        resolveSym(api.lib, "FMOD_Channel_GetDSPClock", "FMOD5_Channel_GetDSPClock"));
    api.setDelay = reinterpret_cast<FN_SetDelay>(
        resolveSym(api.lib, "FMOD_Channel_SetDelay", "FMOD5_Channel_SetDelay"));
    api.getSoftFmt = reinterpret_cast<FN_GetSoftwareFormat>(
        resolveSym(api.lib, "FMOD_System_GetSoftwareFormat", "FMOD5_System_GetSoftwareFormat"));

    api.setVolumeSym = reinterpret_cast<FN_SetVolume>(
        resolveSym(api.lib, "_ZN4FMOD14ChannelControl9setVolumeEf", "FMOD_Channel_SetVolume"));
    if (!api.setVolumeSym)
        api.setVolumeSym = reinterpret_cast<FN_SetVolume>(
            resolveSym(api.lib, "FMOD5_Channel_SetVolume", nullptr));

    api.playSoundSym = reinterpret_cast<FN_PlaySound>(
        resolveSym(api.lib, "_ZN4FMOD6System9playSoundEPNS_5SoundEPNS_12ChannelGroupEbPPNS_7ChannelE",
                   "FMOD_System_PlaySound"));
    api.setPitchSym = reinterpret_cast<FN_SetPitch>(
        resolveSym(api.lib, "_ZN4FMOD14ChannelControl8setPitchEf", "FMOD_Channel_SetPitch"));
    api.set3DAttrSym = reinterpret_cast<FN_Set3DAttributes>(
        resolveSym(api.lib, "_ZN4FMOD14ChannelControl15set3DAttributesEPK11FMOD_VECTORS3_",
                   "FMOD_Channel_Set3DAttributes"));

    SPL_LOGI("FMOD resolved LP=%d REV=%d OCC=%d vol=%d play=%d delay=%d",
             api.setLPGain ? 1 : 0, api.setReverb ? 1 : 0, api.setOcclusion ? 1 : 0,
             api.setVolumeSym ? 1 : 0, api.playSoundSym ? 1 : 0, api.setDelay ? 1 : 0);
    return api.setVolumeSym != nullptr;
}

float distanceListener(const Api& api, void* channel) {
    if (!api.get3DAttr || !channel) return -1.f;
    FMOD_VECTOR pos{}, vel{};
    if (api.get3DAttr(channel, &pos, &vel) != 0) return -1.f;

    FMOD_VECTOR lpos{}, lvel{}, lfwd{}, lup{};
    if (api.getSystem && api.getListener) {
        void* sys = nullptr;
        if (api.getSystem(channel, &sys) == 0 && sys) {
            if (api.getListener(sys, 0, &lpos, &lvel, &lfwd, &lup) == 0) {
                const float dx = pos.x - lpos.x;
                const float dy = pos.y - lpos.y;
                const float dz = pos.z - lpos.z;
                return std::sqrt(dx * dx + dy * dy + dz * dz);
            }
        }
    }
    return std::sqrt(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z);
}

} // namespace spl::fmodapi
