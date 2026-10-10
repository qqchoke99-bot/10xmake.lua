# Signature shortcut (from libSoundPhysicz.so)

No Zaphkiel `getBlock` / `getBlockFromRay` strings required.

Patterns are embedded in `src/core/memory/Signatures.cpp` and scanned at enable:

| Id | Role |
|----|------|
| ClientUpdate | tick host |
| GetAttribute / Current | attributes |
| ActorIsPlayer | filter local player |
| HitResultGetEntity | hit → entity |
| GetBlockEntity | block entity access |
| BarcCtor | region/block helper |
| ScreenViewRender | UI/render (optional) |

On enable, logcat shows:
```
sig[N] scan = 0x...
```
or `NOT FOUND` per id.

FMOD hooks work independently. Block occlusion improves once GetBlockEntity /
related addresses resolve on your build.
