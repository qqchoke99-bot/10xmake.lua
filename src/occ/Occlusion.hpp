#pragma once
#include "core/Types.hpp"

namespace sp::occ {

// Java SoundPhysics.calculateOcclusion + environment → AcousticResult
AcousticResult evaluate(const Vec3& listener, const Vec3& source,
                        const Vec3& listenerVel, const Vec3& sourceVel);

void tick();

} // namespace sp::occ
