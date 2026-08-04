cmake_minimum_required(VERSION 3.22.1)
project(UT99DCAndroid LANGUAGES C CXX)

if(NOT ANDROID)
    message(FATAL_ERROR "This CMake entry point is intended for the Android NDK only.")
endif()

if(NOT ANDROID_ABI STREQUAL "armeabi-v7a" AND NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "UT99DC Android supports armeabi-v7a and arm64-v8a, not ${ANDROID_ABI}.")
endif()

# ANDROID_ARM_MODE/ANDROID_ARM_NEON are Android-toolchain configuration
# variables for 32-bit ARM only. Supplying them globally makes CMake abort
# while configuring arm64-v8a. Preserve the former full ARM instruction mode
# for the OUYA/armeabi-v7a build as a compiler option after project() instead.
if(ANDROID_ABI STREQUAL "armeabi-v7a")
    add_compile_options(-marm)
    message(STATUS "UT99 Android ABI: armeabi-v7a (ARM instruction mode, no mandatory NEON)")
else()
    message(STATUS "UT99 Android ABI: arm64-v8a (native AArch64; Android API 21 minimum)")
endif()

# The visible in-game build label is supplied at runtime by GameActivity
# from BuildConfig.VERSION_NAME.  Do not hardcode it in CMake: an old .cxx
# cache could otherwise leave the native menu label on an earlier version.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(THIRD_PARTY_DIR "${PROJECT_ROOT}/third_party")
set(SDL2_ROOT "${THIRD_PARTY_DIR}/SDL2")
set(UT99DC_ROOT "${THIRD_PARTY_DIR}/ut99dc")
set(UT99DC_SOURCE_DIR "${UT99DC_ROOT}/Source")

# UT99_ANDROID_UMX_LIBXMP_V4_BEGIN
# Link libxmp like the working Unreal Android 1.4.3 music path: as a real
# native dependency, not via dlopen().  The .so is supplied by the user under
# app/src/main/jniLibs/${ANDROID_ABI}/libxmp.so and is also packaged by Gradle.
set(UT99_LIBXMP_SO "${CMAKE_CURRENT_LIST_DIR}/../jniLibs/${ANDROID_ABI}/libxmp.so")
set(UT99_LIBXMP_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/android/include")
if(EXISTS "${UT99_LIBXMP_SO}")
    add_library(ut99_libxmp SHARED IMPORTED GLOBAL)
    set_target_properties(ut99_libxmp PROPERTIES
        IMPORTED_LOCATION "${UT99_LIBXMP_SO}"
        INTERFACE_INCLUDE_DIRECTORIES "${UT99_LIBXMP_INCLUDE_DIR}"
    )
    set(LIBXMP_INCLUDE_DIR "${UT99_LIBXMP_INCLUDE_DIR}" CACHE PATH "UT99 Android libxmp include dir" FORCE)
    set(LIBXMP_LIBRARY ut99_libxmp CACHE STRING "UT99 Android libxmp imported target" FORCE)
    message(STATUS "UT99 Android music: using prebuilt libxmp at ${UT99_LIBXMP_SO}")
else()
    message(WARNING "UT99 Android music: libxmp.so not found at ${UT99_LIBXMP_SO}; UMX music support will be compiled out.")
endif()
# UT99_ANDROID_UMX_LIBXMP_V4_END


add_library(ut99dc_android_bridge SHARED android_bridge.cpp)
target_link_libraries(ut99dc_android_bridge android log)

if(NOT EXISTS "${SDL2_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "SDL2 is missing. Run tools/fetch_deps.ps1 from the project root first.")
endif()

if(NOT EXISTS "${UT99DC_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "ut99dc source is missing. Run tools/fetch_deps.ps1 from the project root first.")
endif()

# SDL2 as shared lib. SDLActivity will load libSDL2.so before the game library.
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${SDL2_ROOT}" "${CMAKE_BINARY_DIR}/SDL2")

if(TARGET SDL2::SDL2)
    set(UT99_ANDROID_SDL_LIB SDL2::SDL2)
elseif(TARGET SDL2)
    set(UT99_ANDROID_SDL_LIB SDL2)
else()
    message(FATAL_ERROR "SDL2 CMake target not found after add_subdirectory(${SDL2_ROOT}).")
endif()

set(SDL2_INCLUDE_DIR "${SDL2_ROOT}/include" CACHE PATH "" FORCE)
set(SDL2_LIBRARY ${UT99_ANDROID_SDL_LIB} CACHE STRING "" FORCE)

# ut99dc/Source/CMakeLists.txt expects its helper modules via CMAKE_MODULE_PATH.
# In this Android wrapper CMAKE_SOURCE_DIR points to app/src/main/cpp, not ut99dc/Source,
# so add the real module directory explicitly before add_subdirectory(ut99dc/Source).
list(APPEND CMAKE_MODULE_PATH "${UT99DC_SOURCE_DIR}/cmake")

# Force the Dreamcast-derived source tree into an Android/SDL/GLES profile.
# The current ut99dc master has no Source/SoundDrv directory, so keep BUILD_NULLSOUNDDRV OFF
# for the first Android configure pass. Audio can be added later via SDL/OpenSL ES.
set(USE_SDL ON CACHE BOOL "" FORCE)
set(LOW_MEMORY ON CACHE BOOL "" FORCE)
set(BUILD_UNREALTOURNAMENT ON CACHE BOOL "" FORCE)
set(BUILD_EDITOR OFF CACHE BOOL "" FORCE)
set(BUILD_NOPENGLDRV OFF CACHE BOOL "" FORCE)
set(BUILD_PVRDRV OFF CACHE BOOL "" FORCE)
set(BUILD_NULLSOUNDDRV OFF CACHE BOOL "" FORCE)
set(BUILD_AICADRV OFF CACHE BOOL "" FORCE)
set(BUILD_DCUTIL OFF CACHE BOOL "" FORCE)

# v19: Android loads the final libUnrealTournament.so via SDLActivity.
# UE1 native packages such as NSDLDrv still expect Unreal's package loader to find
# registered native packages during StaticLoadClass/Engine->Init. Android's Java
# System.loadLibrary path is not enough for that old package loader, so link all
# native packages into the final SDL_main library like the Dreamcast build does.
set(BUILD_STATIC ON CACHE BOOL "" FORCE)

set(PLATFORM_DREAMCAST OFF CACHE BOOL "" FORCE)

# Android-specific defines used by small compatibility shims and guarded patches.
add_compile_definitions(
        PLATFORM_ANDROID=1
        ANDROID=1
        PLATFORM_LITTLE_ENDIAN=1
)

# UT99_ANDROID_V175_64BIT_PORT:
# UE1 native/script mirror classes require 4-byte packing, but applying
# -fpack-struct=4 globally also changes unrelated C/C++ and system structures.
# The 64-bit port now applies explicit #pragma pack(push,4) only around the
# native mirror declarations. Keep only the diagnostic suppression here.
add_compile_options(
        -Wno-address-of-packed-member
)

add_subdirectory("${UT99DC_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/ut99dc")

# UT99_ANDROID_V67_AUDIO_SINGLE_LINK_FIX:
# Do NOT link Audio again from this Android wrapper.
# UnrealTournament/CMakeLists.txt already injects Audio into the static UE package
# whole-archive list. Adding another whole-archive Audio link here pulls the same
# archive members twice and causes duplicate symbols such as GLoadedAudio,
# AudioDevice, ReadMem, AudioLock, etc.

# ut99dc desktop/Dreamcast CMake intentionally removes the shared-library prefix.
# In v19 only UnrealTournament remains a shared Android entry library; the engine
# packages are statically linked into it. Keep the Android-visible library named
# libUnrealTournament.so so SDLActivity can load it normally.
if(TARGET UnrealTournament)
    set_target_properties(UnrealTournament PROPERTIES PREFIX "lib")
endif()

# v17/v18: Launch.cpp emits Android logcat breadcrumbs. The ut99dc launcher target
# already uses the plain target_link_libraries() signature in its own CMake file,
# so this Android wrapper must use the same plain signature too.
if(TARGET UnrealTournament)
    target_link_libraries(UnrealTournament log android dl)
endif()
