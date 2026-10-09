#pragma once

#include <cstdint>
#include <dlfcn.h>
#include <pl/memory/Hook.hpp>

namespace spl::hooks {

inline void* openLibrary(const char* name) {
    if (!name) return nullptr;
    void* h = dlopen(name, RTLD_NOW | RTLD_NOLOAD);
    return h ? h : dlopen(name, RTLD_NOW);
}

inline void* symbol(void* handle, const char* name) {
    return (handle && name) ? dlsym(handle, name) : nullptr;
}

inline bool install(void* target, void* detour, void** original) {
    if (!target || !detour || !original) return false;
    // preloader: 0 = success (same convention as Natural Camera)
    if (pl::memory::hook(target, detour, original) != 0) return false;
    return *original != nullptr;
}

inline void remove(void* target, void* detour) {
    if (target && detour) pl::memory::unhook(target, detour);
}

inline bool addrInMaps(uintptr_t addr) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return false;
    char line[512];
    bool ok = false;
    while (fgets(line, sizeof(line), f)) {
        uintptr_t a = 0, b = 0;
        if (sscanf(line, "%lx-%lx", &a, &b) == 2 && addr >= a && addr < b) {
            ok = true;
            break;
        }
    }
    fclose(f);
    return ok;
}

} // namespace spl::hooks
