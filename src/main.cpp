#include "module/SoundPhysicsModule.hpp"
#include <pl/Mod.hpp>
#include <soundphysics/Log.hpp>

class SoundPhysicsLiteMod {
public:
    static SoundPhysicsLiteMod& instance() {
        static SoundPhysicsLiteMod i;
        return i;
    }

    SoundPhysicsLiteMod() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load() { return spl::SoundPhysicsModule::instance().load(); }
    bool enable() { return spl::SoundPhysicsModule::instance().enable(); }
    bool disable() { return spl::SoundPhysicsModule::instance().disable(); }

private:
    ll::mod::NativeMod& mSelf;
};

PL_REGISTER_MOD(SoundPhysicsLiteMod, SoundPhysicsLiteMod::instance())
