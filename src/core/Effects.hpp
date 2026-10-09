#pragma once

#include "FmodApi.hpp"

namespace spl::effects {

// Confirmed RE (do NOT hook Script binder addresses):
// - FMOD path: setVolume / set3DOcclusion / SetLowPassGain / SetReverb / SetDelay via dlsym
// - playSound chain ~0x110cb390 (set3DAttributes → setVolume → setPitch)
// - getBlockFromRay string 0x2510334 xref 0xe2b7d14 = Script API registration only
// - getBlockFromViewVector string 0x239c536 xref 0xe09b0fc = Script API only
// - blockCost/ClientUpdate/BlockSource plain names: stripped / not present
// Wall raycast = future (signature scan). Current FX = distance real-time.

struct Config {
    float refDist = 48.0f;

    bool soundOcclusionEnabled = true;
    float occDirectNear = 0.0f;
    float occDirectFar = 0.78f;
    float occReverbNear = 0.0f;
    float occReverbFar = 0.58f;

    bool lowOcclusionEnabled = true;
    float lowpassNear = 0.98f;
    float lowpassFar = 0.12f;

    bool volumeOcclusionEnabled = true;
    float volOccNear = 1.0f;
    float volOccFar = 0.28f;

    bool roomReverbEnabled = true;
    float roomWetScale = 1.20f;
    float roomNear = 0.06f;
    float roomWetMax = 0.98f;
    float roomHalfDist = 26.0f;
    float roomEarlyRatio = 0.38f;
    float roomLateRatio = 0.58f;

    float speedOfSound = 343.0f;
    bool speedDelayEnabled = true;
    float maxTravelDelaySec = 0.40f;

    bool echoEnabled = true;
    float echoStrength = 0.92f;
    float echoMinDist = 5.0f;
    float echoHalfDist = 30.0f;
};

float apply(const fmodapi::Api& api, void* channel, float volume, const Config& cfg);

} // namespace spl::effects
