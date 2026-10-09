#pragma once

#include "FmodApi.hpp"
#include "Config.hpp"

namespace spl::effects {

// Apply SPR-style formulas to channel. Returns volume after attenuation/occlusion.
float apply(const fmodapi::Api& api, void* channel, float volume, const cfg::Settings& s);

} // namespace spl::effects
