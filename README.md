<img width="150" height="150" alt="ut99" src="https://github.com/user-attachments/assets/20ab1f2c-fe07-48e2-b161-afabfe6e4cf3" />


# Unreal Tournament (UT99) Android

<p align="left">
  <a href="README.md">README</a>
  &nbsp;|&nbsp;
  <a href="ROADMAP.md">ROADMAP</a>
  &nbsp;|&nbsp;
  <a href="https://github.com/Andiweli/UT99-Android/releases">DOWNLOAD</a>
</p>

![Android 13](https://img.shields.io/badge/up%20to-Android%2013-green)
![ABI](https://img.shields.io/badge/ABI-armeabi--v7a-orange)
![Engine](https://img.shields.io/badge/engine-Unreal%20Engine%201-blue)
![Controller](https://img.shields.io/badge/controls-Touch/Controller-blueviolet)
![Multiplayer](https://img.shields.io/badge/multiplayer-local%20WiFi-blueviolet)


> [!NOTE]
> Unofficial fan port.
> No game data included.
> Requires legally obtained Unreal Tournament v1.400 game files.
> This project is not official and is not endorsed by Epic Games.

> [!IMPORTANT]
> **This app is 32 bit only!** It won't install on your phone? Then it might only accept 64 bit apps. 
> There are no plans on making this app compatible with 64 bit only CPUs. 
> 
> Video games on smartphones are great, but not user-friendly if they require more than two thumbs to control. For this reason, this port is designed for controller input and does only offer basic touchscreen controls.
>
> This project is for preservation, experimentation and personal use only.  
> Unreal Tournament, Unreal Engine and related trademarks are owned by Epic Games.  
> This project is not affiliated with or endorsed by Epic Games.

---

<p align="center">
<a href="https://ibb.co/7xN7Ddcr"><img src="https://i.ibb.co/wZrDqFHp/screen-A.png" alt="screen-A" border="0" width="320" height="180"></a>
<a href="https://ibb.co/DHHZ8PVy"><img src="https://i.ibb.co/fzzsnV15/screen-C.png" alt="screen-C" border="0" width="320" height="180"></a>
<a href="https://ibb.co/b5LxSxxw"><img src="https://i.ibb.co/chtPGPPf/screen-B.png" alt="screen-B" border="0" width="320" height="180"></a>
</p>

---

## ◈ Features

- Android support for newer Android devices up to Android 13 (no 64 bit devices).
- OUYA (Android 4.x) legacy support - with a lower internal render resolution for better performance on legacy hardware.
- Improved Game Data Import – Unreal Tournament data can be imported via folder or ZIP selection and automatically installs to the app's data folder.
- Android 8+ Storage Access Fixed – SAF support added for modern Android versions where direct SD/file access is restricted.
- Legacy storage behavior friendly for old sideload devices (place game data on your microSD/UT99 folder).
- Local WiFi multiplayer and botmatches are available.
- Added touch controls for use without a controller.

> [!NOTE]
> Expect occasional issues, especially on very old Android devices or unusual controller mappings.

---

## ▣ Requirements

- Android device with OpenGL ES 2.0 support.
- Android 4.1 / API 16 or newer.
- ARMv7 compatible device for the current build.
- Android-compatible game controller recommended.
- Original Unreal Tournament / UT99 [PC game data v400](https://archive.org/download/ut-99_202512/UT99.iso).

Required game data folders:

```text
UnrealTournament/
├── System/
├── Maps/
├── Textures/
├── Sounds/
└── Music/
```

The installer accepts the folders either directly at the selected root or inside one top-level Unreal Tournament folder.

---

## ◎ Installation

1. Install the APK on your Android device.
2. Copy your Unreal Tournament game data to your device, either:
   - as an extracted folder, or
   - as a ZIP file.
3. Start **Unreal Tournament**.
4. If no game data is found, the installer screen appears.
5. Choose one of the following:
   - **Select UT99 folder**
   - **Select UT99 ZIP**
6. Wait until the import is finished.
7. The game starts automatically once the required data is found.

The app installs the game data into its private Android data folder.

---

## ◇ Default controller layout

The default controller mapping is designed for Android gamepads, OUYA and handheld devices such as Retroid-style controllers.

| Control | Action |
|---|---|
| Left Stick | Move forward / backward / strafe |
| Right Stick | Look / turn |
| Left Trigger | Alternate Fire |
| Right Trigger | Fire |
| Left Shoulder | Previous Weapon |
| Right Shoulder | Next Weapon |
| A / right face button | Jump |
| B / bottom face button / OUYA O | Crouch |
| Y / left face button / OUYA U | Walk |
| X / top face button / OUYA Y | Wave |
| D-Pad | Menu navigation / in-game navigation depending on context |
| Start | Pause / menu |
| Back | Back / cancel depending on context |

> [!NOTE]
> Button names can differ between Android controllers.  
> If movement or looking feels wrong, open the in-game controls menu and reassign the affected controls.

---

## ▣ Game data notes

Game data is not bundled with this repository.

You need to provide your own legal copy of Unreal Tournament / UT99.  
The Android installer checks for the required folders:

```text
System
Maps
Textures
Sounds
Music
```

If these folders are missing, the game will not start and the installer screen will ask you to select a valid folder or ZIP file.

---

## ◈ Building from source

This project is intended to be built with Android Studio.

General setup:

1. Clone the repository.
2. Open the project in Android Studio.
3. Make sure the Android SDK, NDK and CMake are installed.
4. Fetch or provide required third-party dependencies such as SDL2 if they are not already present.
5. Build the `app` module.

Current Android build characteristics:

```text
Application ID: com.ast.ut99
Minimum SDK:   16
Target SDK:    28
Compile SDK:   33
ABI:           armeabi-v7a
Renderer:      OpenGL ES 2.0
```

---

## ▣ Credits

This Android port is based on Unreal Engine 1 / Unreal Tournament source code work and SDL/OpenGL ES based mobile porting efforts.

Special thanks to the Unreal Engine 1 preservation and porting community.

---

## ◈ Legal

Unreal Tournament, Unreal Engine and related names, assets and trademarks are property of Epic Games.

This repository does **not** include commercial game data.  
You must own a legal copy of Unreal Tournament / UT99 to use this port.

Do not use this project for commercial purposes.
