project(UnrealTournament CXX)

set(SRC_FILES "Src/Launch.cpp")

if(TARGET_IS_DREAMCAST AND DREAMCAST_NO_WHOLE_ARCHIVE)
  list(APPEND SRC_FILES "Src/ForceLink.cpp")
endif()

# Android cannot package a native executable as the SDL entry point.
# SDLActivity loads shared libraries and calls SDL_main from the final game lib.
if(ANDROID)
  add_library(${PROJECT_NAME} SHARED ${SRC_FILES})
else()
  add_executable(${PROJECT_NAME} ${SRC_FILES})
endif()

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE UNREAL_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})

# Select the appropriate viewport / window driver.
# v20: in BUILD_STATIC mode NSDLDrv/KOSDrv must NOT be linked once here and
# again through the whole-archive package list below. Linking the same static
# archive twice caused duplicate symbols such as GStaticExportUNSDLViewport and
# UNSDLViewport::PrivateStaticClass on Android.
if(USE_SDL)
  target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARY})
  target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIR})
  if(NOT BUILD_STATIC)
    target_link_libraries(${PROJECT_NAME} NSDLDrv)
  endif()
elseif(TARGET_IS_DREAMCAST)
  if(NOT BUILD_STATIC)
    target_link_libraries(${PROJECT_NAME} KOSDrv)
  endif()
else()
  # Non-Windows, non-Dreamcast platforms currently only support the SDL path.
  message(FATAL_ERROR "Platforms other than Windows and Dreamcast can only use NSDLDrv.")
endif()

if(BUILD_STATIC)
  set(LINK_PACKAGES ${INSTALL_TARGETS})
  # UT99_ANDROID_V65_AUDIO_LINK_FIX:
  # Some Android incremental CMake builds kept the old INSTALL_TARGETS value and
  # linked Launch.cpp without the Audio static package, producing undefined
  # GLoadedAudio/autoclassUGenericAudioSubsystem references.  Make Audio explicit
  # here whenever the target exists.
  if(TARGET Audio)
    list(APPEND LINK_PACKAGES Audio)
  endif()
  list(REMOVE_DUPLICATES LINK_PACKAGES)
  list(REMOVE_ITEM LINK_PACKAGES ${PROJECT_NAME})
  if(BUILD_DCUTIL)
    list(REMOVE_ITEM LINK_PACKAGES DCUtil)
  endif()

  # Keep SDL2 outside whole-archive. Only UE native packages belong here.
  if(TARGET_IS_DREAMCAST AND DREAMCAST_NO_WHOLE_ARCHIVE)
    target_link_libraries(${PROJECT_NAME} ${LINK_PACKAGES})
  else()
    target_link_libraries(${PROJECT_NAME} -Wl,--whole-archive ${LINK_PACKAGES} -Wl,--no-whole-archive)
  endif()
else()
  target_link_libraries(${PROJECT_NAME} Engine Core)
endif()

if(ANDROID)
  target_link_libraries(${PROJECT_NAME} android log)
endif()

if(TARGET_IS_DREAMCAST AND DREAMCAST_USE_FATFS)
  target_link_libraries(${PROJECT_NAME} fatfs)
endif()
