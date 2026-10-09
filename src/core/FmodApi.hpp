#pragma once

#include <soundphysics/FmodTypes.hpp>

namespace spl::fmodapi {

struct Api {
    void* lib = nullptr;
    FN_SetLowPassGain setLPGain = nullptr;
    FN_SetReverbProps setReverb = nullptr;
    FN_Set3DOcclusion setOcclusion = nullptr;
    FN_Get3DAttributes get3DAttr = nullptr;
    FN_GetSystemObject getSystem = nullptr;
    FN_Get3DListenerAttributes getListener = nullptr;
    FN_GetDSPClock getDSPClock = nullptr;
    FN_SetDelay setDelay = nullptr;
    FN_GetSoftwareFormat getSoftFmt = nullptr;
    FN_SetVolume setVolumeSym = nullptr;
    FN_PlaySound playSoundSym = nullptr;
    FN_SetPitch setPitchSym = nullptr;
    FN_Set3DAttributes set3DAttrSym = nullptr;
};

bool resolve(Api& api);
float distanceListener(const Api& api, void* channel);

} // namespace spl::fmodapi
