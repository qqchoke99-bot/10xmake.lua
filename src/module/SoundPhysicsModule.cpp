#include "SoundPhysicsModule.hpp"
#include "core/Hooks.hpp"
#include "core/Config.hpp"
#include <soundphysics/Log.hpp>
#include <unistd.h>

namespace spl {

SoundPhysicsModule& SoundPhysicsModule::instance() {
    static SoundPhysicsModule m;
    return m;
}

int SoundPhysicsModule::detourSetVolume(void* channel, float volume) {
    auto& self = instance();
    auto& s = cfg::get();
    float outVol = volume;
    if (channel && s.enabled) outVol = effects::apply(self.mApi, channel, volume, s);
    int r = 0;
    if (self.mOrigSetVolume) r = self.mOrigSetVolume(channel, outVol);
    return r;
}

bool SoundPhysicsModule::installSetVolumeHook() {
    if (mHooked.load()) return true;
    if (!fmodapi::resolve(mApi) || !mApi.setVolumeSym) return false;

    void* target = reinterpret_cast<void*>(mApi.setVolumeSym);
    if (!spl::hooks::addrInMaps(reinterpret_cast<uintptr_t>(target))) {
        SPL_LOGW("setVolume not in maps yet");
        return false;
    }

    void* orig = nullptr;
    if (!spl::hooks::install(target, reinterpret_cast<void*>(&detourSetVolume), &orig) || !orig) {
        SPL_LOGW("setVolume hook failed");
        return false;
    }
    mOrigSetVolume = reinterpret_cast<FN_SetVolume>(orig);
    mSetVolumeTarget = target;
    mHooked.store(true);
    SPL_LOGI("HOOKED setVolume — SPR formulas + config toggles");
    return true;
}

void* SoundPhysicsModule::retryMain(void* arg) {
    auto* self = static_cast<SoundPhysicsModule*>(arg);
    for (int i = 0; i < 45 && !self->mStopRetry.load(); ++i) {
        if (self->mHooked.load()) break;
        if (self->installSetVolumeHook()) break;
        sleep(2);
    }
    return nullptr;
}

void SoundPhysicsModule::startRetry() {
    if (mRetryStarted || mHooked.load()) return;
    mRetryStarted = true;
    mStopRetry.store(false);
    if (pthread_create(&mRetryThread, nullptr, &retryMain, this) != 0) {
        mRetryStarted = false;
        return;
    }
    pthread_detach(mRetryThread);
}

void SoundPhysicsModule::tryLoadConfig() {
    cfg::resetDefaults();
    // Common Levi / Android paths
    const char* paths[] = {
        "/data/data/org.levimc.launcher/files/mods/soundphysicslite/config/config.json",
        "/storage/emulated/0/Android/media/org.levimc.launcher/mods/soundphysicslite/config/config.json",
        "/storage/emulated/0/games/levi/mods/soundphysicslite/config/config.json",
        "config/config.json",
        nullptr,
    };
    for (int i = 0; paths[i]; ++i) {
        if (cfg::loadFromFile(paths[i])) return;
    }
    SPL_LOGI("Using embedded SPR defaults (global_intensity=%.2f)", cfg::get().globalIntensity);
}

bool SoundPhysicsModule::load() {
    SPL_LOGI("SPL v0.23 — Sound Physics Remastered formulas + config");
    tryLoadConfig();
    fmodapi::resolve(mApi);
    installSetVolumeHook();
    startRetry();
    return true;
}

bool SoundPhysicsModule::enable() {
    SPL_LOGI("SPL enable");
    tryLoadConfig();
    fmodapi::resolve(mApi);
    installSetVolumeHook();
    startRetry();
    return true;
}

bool SoundPhysicsModule::disable() {
    mStopRetry.store(true);
    if (mHooked.load() && mSetVolumeTarget && mOrigSetVolume) {
        spl::hooks::remove(mSetVolumeTarget, reinterpret_cast<void*>(&detourSetVolume));
        mHooked.store(false);
        mOrigSetVolume = nullptr;
        SPL_LOGI("SPL unhooked");
    }
    return true;
}

} // namespace spl
