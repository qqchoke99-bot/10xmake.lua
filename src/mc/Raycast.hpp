#pragma once
#include "core/Types.hpp"
#include <cstdint>

namespace sp::mc {

extern void* g_blockSource;

// Bedrock-style: Block const* getBlock(BlockSource*, BlockPos const&)
// We use a simplified (bs, x, y, z) shim when only a raw pointer is known.
using GetBlockFn = void* (*)(void* blockSource, int x, int y, int z);
extern GetBlockFn g_getBlock;

// Raw resolved addresses (for advanced / future ABI)
extern void* g_getBlockEntityRaw;
extern void* g_getBlockRaw;

bool resolveWorldAccess(); // Approach A: signature from libSoundPhysicz
bool worldReady();

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

// Optional: set BlockSource from outside (e.g. player dimension hook)
void setBlockSource(void* bs);

} // namespace sp::mc
