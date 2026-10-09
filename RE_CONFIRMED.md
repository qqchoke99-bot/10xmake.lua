# Confirmed RE (this build)

## FMOD (use)
- setVolume hook entry
- set3DOcclusion, SetLowPassGain, SetReverbProperties, SetDelay
- Get3DAttributes + Get3DListenerAttributes for distance
- playSound chain ~0x110cb390 (reference only, not hooked — avoids silence)

## Script ray (do NOT hook)
| String | VA | Xref fn | Kind |
|--------|-----|---------|------|
| getBlockFromRay | 0x2510334 | 0xe2b7d14 | Script binder (location/direction/options) |
| getBlockFromViewVector | 0x239c536 | 0xe09b0fc | Script binder |
| BlockRaycastHit | 0x245965e | 0xe291b84 | Script type registration |

## Not found as plain strings
blockCost, ClientUpdate, BlockSource, HitResult

## Executable ranges (this so)
0x0 – 0x129c4ac0
0x135ec000 – 0x135ed000
