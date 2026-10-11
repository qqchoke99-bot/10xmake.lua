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

// Preloader expects a plain class + PL_REGISTER_MOD(Type, instance())
// not inheritance from pl::Mod (see CameraOverhaul / SoundPhysicsLite).
class SoundPhysicsMod {
public:
    static SoundPhysicsMod& instance() {
        static SoundPhysicsMod mod;
        return mod;
    }

    bool load(pl::mod::ModContext& /*context*/) {
        LOGI("SoundPhysics load");
        sp::loadConfig();
        return true;
    }

    bool enable(pl::mod::ModContext& /*context*/) {
        LOGI("SoundPhysics enable — Approach A (libSoundPhysicz signatures)");
        const bool sigs = sp::memory::resolveAll("libminecraftpe.so");
        const bool world = sp::mc::resolveWorldAccess();
        LOGI("sigs_any=%d getBlockRaw=%p GetBlockEntity=%p worldReady=%d",
             sigs,
             sp::mc::g_getBlockRaw,
             reinterpret_cast<void*>(sp::memory::resolve(sp::memory::SigId::GetBlockEntity)),
             sp::mc::worldReady() ? 1 : 0);
        LOGI("Doppler toggle=%d SoS=%.0f | occlusion=%d",
             sp::g_cfg.dopplerEnabled ? 1 : 0,
             sp::g_cfg.speedOfSound,
             sp::g_cfg.occlusionEnabled ? 1 : 0);

        sp::menu::registerAll();
        const bool fmod = sp::fmodx::resolveAndHook();
        LOGI("fmod_hooks=%d", fmod ? 1 : 0);
        return true;
    }

    bool disable(pl::mod::ModContext& /*context*/) {
        sp::fmodx::unhookAll();
        sp::menu::unregisterAll();
        sp::memory::clear();
        sp::mc::g_blockSource = nullptr;
        sp::mc::g_getBlock = nullptr;
        sp::mc::g_getBlockRaw = nullptr;
        return true;
    }

    bool unload(pl::mod::ModContext& /*context*/) {
        return true;
    }
};

PL_REGISTER_MOD(SoundPhysicsMod, SoundPhysicsMod::instance())
