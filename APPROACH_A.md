# Approach A — same path as libSoundPhysicz.so

## What runs at enable()
1. Scan `libminecraftpe.so` with VersionedSig-style patterns from SoundPhysicz
2. Prefer `SIG_GET_BLOCK_ENTITY` → `g_getBlockRaw`
3. Install `g_getBlock` shim `(BlockSource*, x,y,z) → Block*`
4. Ray march uses world only when `worldReady()` =
   `g_getBlock && g_blockSource && g_getBlockRaw && useWorldRay`

## Doppler
- Menu: **Doppler** on/off
- JSON: `"dopplerEnabled": true/false`
- FMOD `setPitch` hook respects the flag (same idea as `sp::cfg::dopplerEnabled`)

## BlockSource
`g_blockSource` must point at the live world.
Until a dimension/player hook sets it (`sp::mc::setBlockSource(ptr)`),
rays stay safe (air cost = 0). Signatures still log so you can see
which patterns hit on your build.

## Logcat
```
adb logcat -s SoundPhysics
```
Look for:
- `sig[N] scan=0x...`
- `Approach A: GetBlockEntity @ ...`
- `worldReady=0/1`
