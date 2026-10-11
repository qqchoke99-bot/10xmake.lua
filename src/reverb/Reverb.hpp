#pragma once
#include "core/Types.hpp"

namespace sp::reverb {

using SetReverbPropsFn = int (*)(void* system, int instance, const void* props);
using SetChannelReverbFn = int (*)(void* channel, int instance, float wet);

void setFmod(void* system, SetReverbPropsFn a, SetChannelReverbFn b);
void update(const Vec3& listener);
void onChannel(void* channel, float send);
void shutdown();

} // namespace sp::reverb
