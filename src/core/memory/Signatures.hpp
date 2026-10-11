#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace sp::memory {

enum class SigId : std::uint32_t {
    ClientUpdate = 0,
    GetAttribute,
    GetAttributeCurrent,
    ActorIsPlayer,
    HitResultGetEntity,
    GetBlockEntity,   // primary world block access (SoundPhysicz)
    BarcCtor,
    ScreenViewRender,
    // extra candidates for BlockSource / getBlock style prologues
    GetBlockCandidateA,
    GetBlockCandidateB,
    Count
};

inline constexpr std::size_t SigCount = static_cast<std::size_t>(SigId::Count);

bool resolveAll(std::string_view libraryName = "libminecraftpe.so");
std::uintptr_t resolve(SigId id);
void clear();

} // namespace sp::memory
