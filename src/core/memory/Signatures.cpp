#include "core/memory/Signatures.hpp"

#include <android/log.h>
#include <array>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <unordered_map>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "SoundPhysics", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "SoundPhysics", __VA_ARGS__)

namespace pl::memory {
std::unordered_map<std::string, uintptr_t>
resolveSignatures(const std::vector<std::string>& patterns, const char* library) __attribute__((weak));
}

namespace sp::memory {
namespace {

std::array<std::uintptr_t, SigCount> g_addr{};

struct Def {
    SigId id;
    const char* pattern;
};

// Patterns extracted from libSoundPhysicz.so VersionedSig / code blobs
const Def kDefs[] = {
    {SigId::ClientUpdate,
     "FD 7B BE A9 F3 ?? ?? F9 FD 03 00 91 1F 04 00 71 ?? ?? ?? ?? ?? ?? ?? B0 2A ?? ?? F9"},
    {SigId::GetAttribute,
     "FF 43 01 D1 FD 7B 03 A9 F4 4F 04 A9 FD C3 00 91 54 D0 3B D5 F3 03 01 AA 88 ?? ?? F9 A8 83 1F F8"},
    {SigId::GetAttributeCurrent,
     "F4 4F 05 A9 FD 03 01 91 54 D0 3B D5 F3 03 08 AA 88 ?? ?? F9 A8 83 1F F8 E8 83 00 91"},
    {SigId::ActorIsPlayer,
     "FF 03 01 D1 FD 7B 01 A9 F5 13 00 F9 F4 4F 03 A9 FD 43 00 91 55 D0 3B D5 F4 03 00 AA F3 03 01 AA"},
    {SigId::HitResultGetEntity,
     "08 ?? ?? F9 00 7D 40 BD C0 03 5F D6 08 08 40 A9 E0 03 08 AA"},
    // SIG_GET_BLOCK_ENTITY family
    {SigId::GetBlockEntity,
     "?? ?? ?? D1 ?? ?? ?? A9 ?? ?? ?? F9 ?? ?? ?? A9 ?? ?? ?? 91 55 D0 3B D5 F3 03 08 AA F4 03 00 AA"},
    {SigId::BarcCtor,
     "?? ?? ?? D1 ?? ?? ?? A9 ?? ?? ?? F9 ?? ?? ?? 91 53 D0 3B D5 E8 03 00 AA"},
    {SigId::ScreenViewRender,
     "?? ?? ?? D1 ?? ?? ?? A9 ?? ?? ?? A9 ?? ?? ?? A9 ?? ?? ?? 91 56 D0 3B D5 F3 03 00 AA F4 03 02 AA"},
    // Additional getBlock-ish prologues seen in SP binary dumps
    {SigId::GetBlockCandidateA,
     "FF 43 01 D1 FD 7B 03 A9 F4 4F 04 A9 FD C3 00 91 54 D0 3B D5 F3 03 01 AA"},
    {SigId::GetBlockCandidateB,
     "FD 7B BE A9 F3 ?? ?? F9 FD 03 00 91 1F 04 00 71 ?? ?? ?? ?? ?? ?? ?? 90 2A ?? ?? F9"},
};

struct PatByte { uint8_t v; bool any; };

std::vector<PatByte> parsePattern(const char* pat) {
    std::vector<PatByte> out;
    const char* p = pat;
    while (*p) {
        while (*p == ' ') ++p;
        if (!*p) break;
        if (*p == '?') {
            out.push_back({0, true});
            if (p[1] == '?') p += 2; else ++p;
        } else {
            char* end = nullptr;
            unsigned long v = strtoul(p, &end, 16);
            out.push_back({static_cast<uint8_t>(v), false});
            p = end;
        }
    }
    return out;
}

bool matchAt(const uint8_t* data, const std::vector<PatByte>& pat) {
    for (size_t i = 0; i < pat.size(); ++i)
        if (!pat[i].any && data[i] != pat[i].v) return false;
    return true;
}

uintptr_t scanModule(const char* soname, const std::vector<PatByte>& pat) {
    if (pat.empty()) return 0;
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        if (line.find(soname) == std::string::npos) continue;
        if (line.find("r-x") == std::string::npos && line.find("r-xp") == std::string::npos) continue;
        uintptr_t start = 0, end = 0;
        if (sscanf(line.c_str(), "%lx-%lx", &start, &end) != 2) continue;
        if (end <= start || end - start < pat.size()) continue;
        const uint8_t* base = reinterpret_cast<const uint8_t*>(start);
        const size_t len = end - start;
        for (size_t i = 0; i + pat.size() <= len; i += 4) { // align scan
            if (matchAt(base + i, pat)) return start + i;
        }
        break;
    }
    return 0;
}

} // namespace

bool resolveAll(std::string_view libraryName) {
    g_addr.fill(0);
    bool any = false;
    const std::string lib(libraryName.empty() ? "libminecraftpe.so" : libraryName);

    if (pl::memory::resolveSignatures) {
        std::vector<std::string> patterns;
        for (const auto& d : kDefs) patterns.emplace_back(d.pattern);
        auto resolved = pl::memory::resolveSignatures(patterns, lib.c_str());
        for (size_t i = 0; i < sizeof(kDefs) / sizeof(kDefs[0]); ++i) {
            auto it = resolved.find(kDefs[i].pattern);
            if (it != resolved.end() && it->second) {
                g_addr[static_cast<size_t>(kDefs[i].id)] = it->second;
                any = true;
                LOGI("sig[%u] preloader=%p", (unsigned)kDefs[i].id, (void*)it->second);
            }
        }
    }

    for (size_t i = 0; i < sizeof(kDefs) / sizeof(kDefs[0]); ++i) {
        if (g_addr[static_cast<size_t>(kDefs[i].id)]) continue;
        auto pat = parsePattern(kDefs[i].pattern);
        uintptr_t a = scanModule(lib.c_str(), pat);
        if (a) {
            g_addr[static_cast<size_t>(kDefs[i].id)] = a;
            any = true;
            LOGI("sig[%u] scan=%p", (unsigned)kDefs[i].id, (void*)a);
        } else {
            LOGE("sig[%u] NOT FOUND", (unsigned)kDefs[i].id);
        }
    }
    return any;
}

std::uintptr_t resolve(SigId id) {
    auto i = static_cast<std::size_t>(id);
    return i < g_addr.size() ? g_addr[i] : 0;
}

void clear() { g_addr.fill(0); }

} // namespace sp::memory
