#pragma once
#include "core/Types.hpp"
#include <cstdint>

namespace sp::mc {

struct BlockPos {
    int x = 0, y = 0, z = 0;
};

// Live world pointer (captured from game / optional hook)
extern void* g_blockSource;

// Optional direct function (signature path); null if using pure vtable
using GetBlockFn = void* (*)(void* blockSource, const BlockPos* pos);
extern GetBlockFn g_getBlock;
extern void* g_getBlockRaw;

bool resolveWorldAccess();
bool worldReady();

void setListener(const Vec3& pos, const Vec3& vel);
void getListener(Vec3& pos, Vec3& vel);

// Returns opaque Block* (or wrapper). null ≈ air / unloaded
void* getBlockAt(int x, int y, int z);

struct Hit {
    bool hit = false;
    float distance = 0.f;
    float cost = 0.f;
    float reflectivity = 0.2f;
};

float blockCost(void* block);
float rayCost(const Vec3& from, const Vec3& to);
float rayDistance(const Vec3& from, const Vec3& to, bool& hit);
Hit rayMarch(const Vec3& from, const Vec3& to, int maxSteps = 24);

} // namespace sp::mc
