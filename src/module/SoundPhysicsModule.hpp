#pragma once

#include "core/Effects.hpp"
#include "core/FmodApi.hpp"
#include <atomic>
#include <pthread.h>

namespace spl {

class SoundPhysicsModule {
public:
    static SoundPhysicsModule& instance();

    bool load();
    bool enable();
    bool disable();

private:
    fmodapi::Api mApi{};
    effects::Config mCfg{};
    FN_SetVolume mOrigSetVolume = nullptr;
    void* mSetVolumeTarget = nullptr;
    std::atomic<bool> mHooked{false};
    std::atomic<bool> mStopRetry{false};
    pthread_t mRetryThread{};
    bool mRetryStarted = false;

    bool installSetVolumeHook();
    void startRetry();
    static void* retryMain(void* arg);
    static int detourSetVolume(void* channel, float volume);
};

} // namespace spl
