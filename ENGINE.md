# Engine: Java Sound Physics Remastered → C++ + libSoundPhysicz layout

## Ported from Java (henkelmax / Sonic Ether)
- `runOcclusion` / `calculateOcclusion` (1 path or 9-variation)
- `directCutoff = exp(-occlusionAccumulation * globalBlockAbsorption)`
- `directGain = pow(directCutoff, 0.1)`
- Environment ray bundle for reverb send (reduced ray count for mobile)
- Config keys aligned with SPR (`maxOcclusionRays`, `maxOcclusion`, `strictOcclusion`, …)

## Layout from libSoundPhysicz.so
- `sp::mc::rayCost` / `rayDistance` / `blockCost` (own march)
- `sp::occ::evaluate` applies FMOD volume / lowpass / reverb / Doppler
- Signature tables for future `GetBlock` bind (`g_getBlock`, `g_blockSource`)

## When world bind is missing
Ray march still runs; without `g_getBlock` every cell is treated as air (cost 0).
Distance attenuation, Doppler, and soft reverb still apply via FMOD hooks.
Once `g_getBlock` + `g_blockSource` are set, wall occlusion becomes real.
