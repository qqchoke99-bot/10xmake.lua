#include "mc/Raycast.hpp"
#include "core/Types.hpp"
#include <android/log.h>
#include <cmath>
#include <cstdint>
#include <mutex>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "SoundPhysics", __VA_ARGS__)

namespace sp::mc {

void* g_blockSource = nullptr;
GetBlockFn g_getBlock = nullptr;
void* g_getBlockRaw = nullptr;

namespace {
std::mutex g_lisMtx;
Vec3 g_lisPos{};
Vec3 g_lisVel{};
bool g_haveLis = false;

// Bedrock BlockSource::getBlock is vtable slot at offset 0x30 (index 6)
constexpr size_t kGetBlockVOff = 0x30;

void* callGetBlockVt(void* bs, const BlockPos& pos) {
    if (!bs) return nullptr;
    void** vt = *reinterpret_cast<void***>(bs);
    if (!vt) return nullptr;
    auto* slot = reinterpret_cast<void*>(vt[kGetBlockVOff / sizeof(void*)]);
    if (!slot) return nullptr;
    auto fn = reinterpret_cast<GetBlockFn>(slot);
    // Prefer resolved symbol if signature path filled it
    if (g_getBlock) fn = g_getBlock;
    void* r = fn(bs, &pos);
    if (!r) return nullptr;
    // Hot path after virtual getBlock often does: ldr x0, [x0, #0x68]
    // Treat non-null at +0x68 as the concrete Block when present.
    void* inner = *reinterpret_cast<void**>(reinterpret_cast<char*>(r) + 0x68);
    return inner ? inner : r;
}

} // namespace

bool resolveWorldAccess() {
    // Vtable path needs only live BlockSource* — filled when first 3D sound
    // runs with bs pointer from hooks / heartbeat, or external set.
    if (g_blockSource) {
        void** vt = *reinterpret_cast<void***>(g_blockSource);
        if (vt && vt[kGetBlockVOff / sizeof(void*)]) {
            g_getBlockRaw = vt[kGetBlockVOff / sizeof(void*)];
            LOGI("getBlock via vtable+0x30 raw=%p bs=%p", g_getBlockRaw, g_blockSource);
            return true;
        }
    }
    if (g_getBlock) {
        LOGI("getBlock via symbol %p", reinterpret_cast<void*>(g_getBlock));
        return true;
    }
    LOGI("getBlock not ready (need live BlockSource*)");
    return false;
}

bool worldReady() {
    return g_blockSource != nullptr &&
           (g_getBlock != nullptr || g_getBlockRaw != nullptr ||
            (g_blockSource && *reinterpret_cast<void***>(g_blockSource)));
}

void setListener(const Vec3& pos, const Vec3& vel) {
    std::lock_guard<std::mutex> lock(g_lisMtx);
    g_lisPos = pos;
    g_lisVel = vel;
    g_haveLis = true;
}

void getListener(Vec3& pos, Vec3& vel) {
    std::lock_guard<std::mutex> lock(g_lisMtx);
    pos = g_lisPos;
    vel = g_lisVel;
}

void* getBlockAt(int x, int y, int z) {
    if (!g_blockSource) return nullptr;
    BlockPos p{x, y, z};
    // Refresh vtable slot once
    if (!g_getBlockRaw) {
        void** vt = *reinterpret_cast<void***>(g_blockSource);
        if (vt) g_getBlockRaw = vt[kGetBlockVOff / sizeof(void*)];
    }
    return callGetBlockVt(g_blockSource, p);
}

float blockCost(void* block) {
    // null / failed read = air
    if (!block) return 0.f;
    // Without full BlockType map: any solid-ish block costs 1 (Java SPR-like default)
    return 1.f;
}

float rayCost(const Vec3& from, const Vec3& to) {
    if (!g_blockSource) return 0.f;
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dz = to.z - from.z;
    const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (dist < 0.01f) return 0.f;

    const int steps = std::max(1, std::min(48, (int)std::ceil(dist * 1.25f)));
    float acc = 0.f;
    int lastX = INT32_MIN, lastY = INT32_MIN, lastZ = INT32_MIN;
    for (int i = 1; i <= steps; ++i) {
        const float t = (float)i / (float)steps;
        const int bx = (int)std::floor(from.x + dx * t);
        const int by = (int)std::floor(from.y + dy * t);
        const int bz = (int)std::floor(from.z + dz * t);
        if (bx == lastX && by == lastY && bz == lastZ) continue;
        lastX = bx;
        lastY = by;
        lastZ = bz;
        void* b = getBlockAt(bx, by, bz);
        acc += blockCost(b);
        if (acc >= 15.f) break; // cap
    }
    return acc;
}

float rayDistance(const Vec3& from, const Vec3& to, bool& hit) {
    hit = false;
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dz = to.z - from.z;
    const float maxD = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (maxD < 0.01f) return 0.f;
    if (!g_blockSource) return maxD;

    const int steps = std::max(1, std::min(48, (int)std::ceil(maxD)));
    int lastX = INT32_MIN, lastY = INT32_MIN, lastZ = INT32_MIN;
    for (int i = 1; i <= steps; ++i) {
        const float t = (float)i / (float)steps;
        const int bx = (int)std::floor(from.x + dx * t);
        const int by = (int)std::floor(from.y + dy * t);
        const int bz = (int)std::floor(from.z + dz * t);
        if (bx == lastX && by == lastY && bz == lastZ) continue;
        lastX = bx;
        lastY = by;
        lastZ = bz;
        if (blockCost(getBlockAt(bx, by, bz)) > 0.01f) {
            hit = true;
            return t * maxD;
        }
    }
    return maxD;
}

Hit rayMarch(const Vec3& from, const Vec3& to, int maxSteps) {
    Hit h{};
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dz = to.z - from.z;
    const float maxD = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (maxD < 0.01f || !g_blockSource) {
        h.distance = maxD;
        return h;
    }
    const int steps = std::max(1, std::min(maxSteps, (int)std::ceil(maxD)));
    int lastX = INT32_MIN, lastY = INT32_MIN, lastZ = INT32_MIN;
    for (int i = 1; i <= steps; ++i) {
        const float t = (float)i / (float)steps;
        const int bx = (int)std::floor(from.x + dx * t);
        const int by = (int)std::floor(from.y + dy * t);
        const int bz = (int)std::floor(from.z + dz * t);
        if (bx == lastX && by == lastY && bz == lastZ) continue;
        lastX = bx;
        lastY = by;
        lastZ = bz;
        void* b = getBlockAt(bx, by, bz);
        const float c = blockCost(b);
        if (c > 0.01f) {
            h.hit = true;
            h.distance = t * maxD;
            h.cost = c;
            h.reflectivity = 0.35f;
            return h;
        }
    }
    h.distance = maxD;
    return h;
}

} // namespace sp::mc
