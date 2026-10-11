#include "mc/Raycast.hpp"
#include "core/memory/Signatures.hpp"
#include <android/log.h>
#include <cmath>
#include <algorithm>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "SoundPhysics", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "SoundPhysics", __VA_ARGS__)

namespace sp::mc {

void* g_blockSource = nullptr;
GetBlockFn g_getBlock = nullptr;
void* g_getBlockEntityRaw = nullptr;
void* g_getBlockRaw = nullptr;

void setBlockSource(void* bs) {
    g_blockSource = bs;
    LOGI("blockSource set %p", bs);
}

// Soft shim: many Bedrock getBlock take (BlockSource*, BlockPos&)
// Without full ABI we attempt a C-style (bs,x,y,z) only when signature
// matches a known simple form. Otherwise ray treats cells as air until
// a proper hook supplies g_getBlock.
static void* shimGetBlockXYZ(void* bs, int x, int y, int z) {
    if (!bs || !g_getBlockRaw) return nullptr;
    // Pack BlockPos on stack: 3x int32
    struct BlockPos { int x, y, z; } pos{x, y, z};
    using Fn = void* (*)(void*, BlockPos const&);
    auto fn = reinterpret_cast<Fn>(g_getBlockRaw);
    // May crash if ABI wrong — only enabled when user sets useWorldRay and
    // we have high confidence. Default path stays safe.
    return fn(bs, pos);
}

bool resolveWorldAccess() {
    g_getBlockEntityRaw = reinterpret_cast<void*>(
        sp::memory::resolve(sp::memory::SigId::GetBlockEntity));
    auto candA = sp::memory::resolve(sp::memory::SigId::GetBlockCandidateA);
    auto candB = sp::memory::resolve(sp::memory::SigId::GetBlockCandidateB);

    // Prefer explicit GetBlockEntity table hit
    if (g_getBlockEntityRaw) {
        g_getBlockRaw = g_getBlockEntityRaw;
        LOGI("Approach A: GetBlockEntity @ %p", g_getBlockEntityRaw);
    } else if (candA) {
        g_getBlockRaw = reinterpret_cast<void*>(candA);
        LOGI("Approach A: GetBlock candidate A @ %p", (void*)candA);
    } else if (candB) {
        g_getBlockRaw = reinterpret_cast<void*>(candB);
        LOGI("Approach A: GetBlock candidate B @ %p", (void*)candB);
    }

    // Only install callable shim if we also have blockSource later
    // For safety: do NOT call into game until blockSource is set
    if (g_getBlockRaw) {
        g_getBlock = &shimGetBlockXYZ;
        LOGI("getBlock shim installed (needs blockSource to be live)");
    } else {
        g_getBlock = nullptr;
        LOGE("Approach A: no getBlock signature — ray cost stays 0 until bind");
    }
    return g_getBlockRaw != nullptr;
}

bool worldReady() {
    return g_getBlock != nullptr && g_blockSource != nullptr && g_getBlockRaw != nullptr;
}

float blockCost(void* blockPtr) {
    if (!blockPtr) return 0.f;
    // Solid default (SoundPhysicz logs per-id; without id table use 1.0)
    return 1.0f;
}

static inline int floori(float v) { return (int)std::floor(v); }

RayHit rayMarch(const Vec3& from, const Vec3& to, int maxSteps) {
    RayHit out;
    const Vec3 delta = to - from;
    const float dist = delta.length();
    if (dist < 1e-4f) return out;

    const float stepSz = std::max(0.25f, sp::g_cfg.stepSize);
    const int steps = std::min(maxSteps, std::max(1, (int)std::ceil(dist / stepSz)));
    const Vec3 step = delta * (1.f / float(steps));

    float acc = 0.f;
    int lastX = floori(from.x), lastY = floori(from.y), lastZ = floori(from.z);
    const bool live = worldReady() && sp::g_cfg.useWorldRay;

    for (int i = 1; i <= steps; ++i) {
        const Vec3 p = from + step * float(i);
        const int bx = floori(p.x), by = floori(p.y), bz = floori(p.z);
        if (bx == lastX && by == lastY && bz == lastZ) continue;
        lastX = bx; lastY = by; lastZ = bz;

        void* blk = nullptr;
        if (live) {
            // Safe call only when both pointers set
            blk = g_getBlock(g_blockSource, bx, by, bz);
        }
        const float c = blockCost(blk);
        if (c > 0.f) {
            acc += c;
            out.hit = true;
            out.reflectivity = 0.55f;
            if (acc >= sp::g_cfg.maxOcclusion) {
                out.distance = (step * float(i)).length();
                out.cost = sp::g_cfg.maxOcclusion;
                return out;
            }
        }
    }
    out.distance = dist;
    out.cost = acc;
    return out;
}

float rayCost(const Vec3& from, const Vec3& to) {
    return rayMarch(from, to, sp::g_cfg.maxOcclusionRays).cost;
}

float rayDistance(const Vec3& from, const Vec3& to, bool& hit) {
    auto h = rayMarch(from, to, sp::g_cfg.maxOcclusionRays);
    hit = h.hit;
    return h.hit ? h.distance : (to - from).length();
}

} // namespace sp::mc
