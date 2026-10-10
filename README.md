# SoundPhysics (Levi / xmake)

Port of Sound Physics Remastered style audio for Bedrock via FMOD hooks.

## Features
| Feature | How |
|---------|-----|
| **Raycast occlusion** | Multi-ray path cost listener→source (+ diffraction side rays) |
| **Occlusion Volume** | Volume cut by blocked path |
| **Occlusion Lowpass** | FMOD LOWPASS DSP cutoff drops when occluded |
| **Room reverb** | Probe rays around listener → FMOD system reverb + channel send |
| **Echo** | Extra wet from reflective probes |
| **Speed of Sound 343** | Doppler pitch uses `c = 343` blocks/s |
| **Doppler** | Relative velocity along LOS |

## Build
xmake android arm64 → `SoundPhysics.levipack`

## Config
`/sdcard/games/SoundPhysics/config.json` or ModMenu toggles.

## Raycast binding (important)
Until `BlockSource` / `getBlockFromRay` is signature-bound, rays treat air as clear
(distance attenuation + Doppler still work). Hook world ray via:

```cpp
sp::mc::setBlockRayFn([](const Vec3& from, const Vec3& to) -> RayHit {
  // call resolved BlockSource raycast
});
```

Use Zaphkiel on `getBlockFromRay` / `Clip` / `BlockSource` for the real ABI.

## Signature shortcut
Patterns from working `libSoundPhysicz.so` are in `src/core/memory/Signatures.cpp`.
On enable they are scanned in `libminecraftpe.so` — no Zaphkiel string targets needed
for the FMOD path. Check logcat `SoundPhysics` for `sig[N] scan=`.
