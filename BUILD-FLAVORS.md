# UT99 Android build flavors

The project contains two platform flavors from one shared Java/JNI/native codebase.

- `normal`: standard Android + OUYA, application ID `com.ast.ut99`, minSdk 16.
- `automotive`: Android Automotive OS, application ID `com.ast.ut99android`.

`automotiveDebug` is intentionally disabled. This leaves `normalDebug` as the only debug/run variant, so a fresh Android Studio import and the normal Run/Play workflow use the standard Android/OUYA build by default.

## Build commands

Normal Android / OUYA APK:

```text
./gradlew buildNormalApk
```

Equivalent Gradle task:

```text
./gradlew assembleNormalDebug
```

Android Automotive release AAB:

```text
./gradlew buildAutomotiveAab
```

Equivalent Gradle task:

```text
./gradlew bundleAutomotiveRelease
```

Outputs are written below `app/build/outputs/`.

## Automotive-specific behavior

The Automotive flavor keeps the shared game/engine sources but activates AAOS behavior at runtime when `android.hardware.type.automotive` is present:

- Android immersive-mode forcing is disabled so vehicle-owned system bars remain under AAOS control.
- Automotive lifecycle cleanup releases pending overlay/UI callbacks when the game is backgrounded.
- SDL touch normalization uses the actual visible `SurfaceView` dimensions instead of the physical display dimensions. This avoids touch-position offsets caused by AAOS system-bar/safe-area regions while leaving the existing Normal/OUYA touch path unchanged.

The project uses Android Gradle Plugin 8.6.1 / Gradle 8.7 and requires JDK 17 or newer.
