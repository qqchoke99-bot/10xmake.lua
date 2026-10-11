#pragma once
#include "core/Types.hpp"
#include <cstdint>

namespace sp::mc {

extern void* g_blockSource;

using GetBlockFn = void* (*)(void* blockSource, int x, int y, int z);
extern GetBlockFn g_getBlock;

extern void* g_getBlockEntityRaw;
extern void* g_getBlockRaw;

bool resolveWorldAccess();
bool worldReady();
void setBlockSource(void* bs);

float blockCost(void* blockPtr);

struct RayHit {
    bool hit = false;
    float distance = 0.f;
    float cost = 0.f;
    float reflectivity = 0.5f;
};

RayHit rayMarch(const Vec3& from, const Vec3& to, int maxSteps);
float rayCost(const Vec3& from, const Vec3& to);
float rayDistance(const Vec3& from, const Vec3& to, bool& hit);

void setListener(const Vec3& pos, const Vec3& vel);
void getListener(Vec3& pos, Vec3& vel);

} // namespace sp::mc
