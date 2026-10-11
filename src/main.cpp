#include "fmod/Hooks.hpp"
#include "core/Types.hpp"
#include "core/memory/Signatures.hpp"
#include "mc/Raycast.hpp"

#include <pl/Mod.hpp>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "SoundPhysics", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "SoundPhysics", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "SoundPhysics", __VA_ARGS__)

namespace pl::memory {
enum class HookPriority : int { Lowest = 0, Low = 100, Normal = 200, High = 300, Highest = 400 };
bool hook(void* target, void* detour, void** original, HookPriority priority);
bool unhook(void* target, void* detour);
}

namespace sp {
extern Config g_cfg;
void loadConfig();
}
namespace sp::menu {
void registerAll();
void unregisterAll();
}

namespace {

using GBEFn = void* (*)(void* self, const void* pos);
GBEFn orig_gbe = nullptr;
void* t_gbe = nullptr;

void* hk_getBlockEntity(void* self, const void* pos) {
    if (self) sp::mc::g_blockSource = self;
    return orig_gbe ? orig_gbe(self, pos) : nullptr;
}

void tryHookGetBlockEntity() {
    const auto addr = sp::memory::resolve(sp::memory::SigId::GetBlockEntity);
    if (!addr) {
        LOGW("GetBlockEntity sig miss — BlockSource capture delayed");
        return;
    }
    t_gbe = reinterpret_cast<void*>(addr);
    if (pl::memory::hook(t_gbe, reinterpret_cast<void*>(&hk_getBlockEntity),
                         reinterpret_cast<void**>(&orig_gbe), pl::memory::HookPriority::Normal)) {
        LOGI("GetBlockEntity hook OK → capture BlockSource* on call");
    } else {
        LOGE("GetBlockEntity hook FAILED");
    }
}

void unhookGetBlockEntity() {
    if (t_gbe && orig_gbe) {
        pl::memory::unhook(t_gbe, reinterpret_cast<void*>(&hk_getBlockEntity));
        t_gbe = nullptr;
        orig_gbe = nullptr;
    }
}

} // namespace

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
        LOGI("SoundPhysics enable — getBlock via BlockSource vtable+0x30");
        const bool sigs = sp::memory::resolveAll("libminecraftpe.so");
        LOGI("sigs_any=%d GetBlockEntity=%p", sigs,
             reinterpret_cast<void*>(sp::memory::resolve(sp::memory::SigId::GetBlockEntity)));

        tryHookGetBlockEntity();
        sp::menu::registerAll();
        const bool fmod = sp::fmodx::resolveAndHook();
        LOGI("fmod_hooks=%d | occlusion uses vtable getBlock when bs live", fmod ? 1 : 0);
        return true;
    }

    bool disable(pl::mod::ModContext& /*context*/) {
        unhookGetBlockEntity();
        sp::fmodx::unhookAll();
        sp::menu::unregisterAll();
        sp::memory::clear();
        sp::mc::g_blockSource = nullptr;
        sp::mc::g_getBlock = nullptr;
        sp::mc::g_getBlockRaw = nullptr;
        return true;
    }

    bool unload(pl::mod::ModContext& /*context*/) { return true; }
};

PL_REGISTER_MOD(SoundPhysicsMod, SoundPhysicsMod::instance())
