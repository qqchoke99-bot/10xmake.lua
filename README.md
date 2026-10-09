# SoundPhysicsLite v0.22

Built from full RE session data (FMOD + Zaphkiel Script ray findings).

## Features
- Sound Occlusion (`set3DOcclusion`)
- Low Occlusion (lowpass / muffle)
- Volume Occlusion (volume scale in setVolume detour)
- Room Reverb continuous (no hard distance cap)
- Echo + Speed of sound **343 blocks/s**

## RE notes (confirmed)
- FMOD via `dlsym` — primary path
- `getBlockFromRay` @ string `0x2510334` / fn `0xe2b7d14` = **Script binder only**
- `getBlockFromViewVector` @ `0x239c536` / fn `0xe09b0fc` = **Script binder only**
- Do **not** hook those binder addresses
- Wall raycast needs native signatures (not in this build)

## Build
```bash
xmake f -y -p android -a arm64-v8a -m release --ndk=$ANDROID_NDK_HOME
xmake -y
```
Or GitHub Actions → `SoundPhysicsLite.levipack`

## Package
```
manifest.json
libSoundPhysicsLite.so
icon.png
```
