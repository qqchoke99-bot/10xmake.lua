#include "fmod/Hooks.hpp"
#include "core/Types.hpp"
#include "core/memory/Signatures.hpp"
#include "mc/Raycast.hpp"

#include <pl/Mod.hpp>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "SoundPhysics", __VA_ARGS__)

namespace sp {
extern Config g_cfg;
void loadConfig();
}
namespace sp::menu {
void registerAll();
void unregisterAll();
}

class SoundPhysicsMod : public pl::Mod {
public:
    bool load() override {
        LOGI("SoundPhysics load");
        sp::loadConfig();
        return true;
    }
    bool enable() override {
        LOGI("SoundPhysics enable — Approach A (libSoundPhysicz signatures)");
        const bool sigs = sp::memory::resolveAll("libminecraftpe.so");
        const bool world = sp::mc::resolveWorldAccess();
        LOGI("sigs_any=%d getBlockRaw=%p GetBlockEntity=%p worldReady=%d",
             sigs,
             sp::mc::g_getBlockRaw,
             (void*)sp::memory::resolve(sp::memory::SigId::GetBlockEntity),
             sp::mc::worldReady());
        LOGI("Doppler toggle=%d SoS=%.0f | occlusion=%d",
             sp::g_cfg.dopplerEnabled, sp::g_cfg.speedOfSound, sp::g_cfg.occlusionEnabled);

        sp::menu::registerAll();
        const bool fmod = sp::fmodx::resolveAndHook();
        LOGI("fmod_hooks=%d", fmod);
        return true;
    }
    bool disable() override {
        sp::fmodx::unhookAll();
        sp::menu::unregisterAll();
        sp::memory::clear();
        sp::mc::g_blockSource = nullptr;
        sp::mc::g_getBlock = nullptr;
        sp::mc::g_getBlockRaw = nullptr;
        return true;
    }
};

PL_REGISTER_MOD(SoundPhysicsMod)
