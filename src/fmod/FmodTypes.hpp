#pragma once

namespace sp::fmodx {

// FMOD_VECTOR layout (x,y,z floats) — matches FMOD C API
struct FmodVector {
    float x;
    float y;
    float z;
};

} // namespace sp::fmodx
