#pragma once

struct FMOD_VECTOR {
    float x, y, z;
};

using FN_SetVolume = int (*)(void* channel, float volume);
using FN_SetLowPassGain = int (*)(void* channel, float gain);
using FN_SetReverbProps = int (*)(void* channel, int instance, float wet);
using FN_Set3DOcclusion = int (*)(void* channel, float direct, float reverb);
using FN_Get3DAttributes = int (*)(void* channel, FMOD_VECTOR* pos, FMOD_VECTOR* vel);
using FN_GetSystemObject = int (*)(void* channel, void** system);
using FN_Get3DListenerAttributes = int (*)(void* system, int listener, FMOD_VECTOR* pos,
                                           FMOD_VECTOR* vel, FMOD_VECTOR* forward, FMOD_VECTOR* up);
using FN_GetDSPClock = int (*)(void* channel, unsigned long long* dspclock, unsigned long long* parentclock);
using FN_SetDelay = int (*)(void* channel, unsigned long long start, unsigned long long end, int stopchannels);
using FN_GetSoftwareFormat = int (*)(void* system, int* samplerate, int* speakermode, int* numrawspeakers);
using FN_PlaySound = int (*)(void* system, void* sound, void* group, int paused, void** channel);
using FN_SetPitch = int (*)(void* channel, float pitch);
using FN_Set3DAttributes = int (*)(void* channel, const FMOD_VECTOR* pos, const FMOD_VECTOR* vel);
