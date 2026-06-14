cmake_minimum_required(VERSION 3.15)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

project(UT99 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)

if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "" FORCE)
  endif()
endif()

# Options

option(USE_SDL "Use NSDLDrv instead of WinDrv" ON)
option(LOW_MEMORY "Conserve memory as much as possible" OFF)
option(BUILD_UNREALTOURNAMENT "Build the launcher" ON)
option(BUILD_EDITOR "Build Editor (GUI support requires Windows)" ON)
option(BUILD_NOPENGLDRV "Build NOpenGLDrv" OFF)
option(BUILD_PVRDRV "Build PVRDrv (Dreamcast only)" OFF)
option(BUILD_NULLSOUNDDRV "Build SoundDrv (Null driver)" ON)
option(BUILD_AICADRV "Build AICADrv (Dreamcast AICA sound driver)" OFF)
option(BUILD_DCUTIL "Build DCUtil" ON)
option(BUILD_STATIC "Link everything into a single binary" OFF)


option(DREAMCAST_USE_FATFS "Try to load data from SD or IDE" OFF)
option(DREAMCAST_USE_FILE_POOL "Use file handle pool (wrap stdio)" ON)
option(DREAMCAST_NO_WHOLE_ARCHIVE "Use a hack to link only actually used code instead of using -whole-archive." OFF)
option(DREAMCAST_BUILD_CDI "Create CDI image after building (uses mkdcdisc)" OFF)

set(DREAMCAST_GAME_DATA "${CMAKE_SOURCE_DIR}/../UT99" CACHE PATH "Path to Unreal game data root (contains System, Maps, etc)")
set(DREAMCAST_IP_BIN "${CMAKE_SOURCE_DIR}/../UT99/IP.BIN" CACHE FILEPATH "Path to IP.BIN for Dreamcast disc")
set(DREAMCAST_CDI_OUTPUT "${CMAKE_SOURCE_DIR}/../UnrealTournament.cdi" CACHE FILEPATH "Output CDI image path")
set(DREAMCAST_IP_TXT "${CMAKE_SOURCE_DIR}/ip.txt" CACHE FILEPATH "Path to ip.txt template for makeip")
set(DREAMCAST_ELF_FILE "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/UnrealTournament.elf" CACHE FILEPATH "Path to Unreal ELF used for mkdcdisc")

if(PLATFORM_DREAMCAST)
  set(BUILD_STATIC ON)
  set(USE_SDL OFF)
  set(LOW_MEMORY ON)
  set(BUILD_EDITOR OFF) 
  set(BUILD_NOPENALDRV OFF)
  set(BUILD_NULLSOUNDDRV OFF)
  set(BUILD_AICADRV ON)
  set(BUILD_NOPENGLESDRV OFF)
  set(BUILD_GLDCDRV OFF)
  set(BUILD_PVRDRV ON)
  set(BUILD_DCUTIL OFF)
endif()


if(BUILD_STATIC)
  if(MSVC)
    message(FATAL_ERROR "Static mode is not supported with MSVC")
  endif()
  set(LIB_TYPE STATIC)
  add_definitions(-DUNREAL_STATIC)
  message(STATUS "Static mode enabled")
else()
  set(LIB_TYPE SHARED)
endif()

# Determine target arch and get normalized arch string

include(TargetArch)
target_architecture(TARGET_ARCH)

set(TARGET_IS_X86 FALSE)
set(TARGET_IS_ARM FALSE)
set(TARGET_IS_64BIT FALSE)
set(TARGET_IS_BIG_ENDIAN FALSE)

if(TARGET_ARCH MATCHES "arm")
  set(TARGET_IS_ARM TRUE)
  if(TARGET_ARCH MATCHES "64")
    # arm64/aarch64
    set(TARGET_IS_64BIT TRUE)
    set(TARGET_ARCH "arm64")
  else()
    # arm
    set(TARGET_ARCH "arm")
  endif()
elseif((TARGET_ARCH MATCHES "86") OR (TARGET_ARCH STREQUAL "ia64"))
  set(TARGET_IS_X86 TRUE)
  if(TARGET_ARCH MATCHES "64")
    # x86_64/amd64
    set(TARGET_IS_64BIT TRUE)
    set(TARGET_ARCH "x86_64")
  else()
    # x86
    set(TARGET_ARCH "x86")
  endif()
endif()

if(PLATFORM_DREAMCAST)
  set(TARGET_ARCH "sh4")
endif()

message(STATUS "Target arch: ${TARGET_ARCH}")
message(STATUS "64-bit: ${TARGET_IS_64BIT}")

if(TARGET_IS_64BIT)
  if(TARGET_IS_X86 AND NOT MSVC)
    message(STATUS "Building x86 binary with x86_64 compiler")
    set(TARGET_IS_64BIT FALSE)
    set(TARGET_ARCH "x86")
    add_compile_options(-m32)
  else()
    message(FATAL_ERROR "64-bit platforms are currently not supported. If you're building on Windows with MSVC, try -A Win32.")
  endif()
endif()

# Determine target OS/platform

set(TARGET_IS_WINDOWS FALSE)
set(TARGET_IS_UNIX FALSE)
set(TARGET_IS_DREAMCAST FALSE)
if(WIN32)
  set(TARGET_IS_WINDOWS TRUE)
else()
  # TODO
  set(TARGET_IS_UNIX TRUE)
endif()

if(PLATFORM_DREAMCAST)
  set(TARGET_IS_DREAMCAST TRUE)
endif()

message(STATUS "Windows target: ${TARGET_IS_WINDOWS}")

# Common compiler options

if(BUILD_FOR_PSVITA AND NOT TARGET_IS_ARM)
  message(FATAL_ERROR "PSVita is ARM.")
endif()

if(TARGET_IS_64BIT)
  add_definitions(-DPLATFORM_64BIT)
else()
  add_definitions(-DPLATFORM_32BIT)
endif()

if(TARGET_IS_X86)
  add_definitions(-DPLATFORM_X86)
elseif(TARGET_IS_DREAMCAST)
  set(DREAMCAST_CFLAGS
    -m4-single -fsigned-char -fno-short-enums -fexceptions -fno-rtti -fsingle-precision-constant -flto=8
    -fomit-frame-pointer -fstack-usage
  )
  set(DREAMCAST_WFLAGS
    -Wno-misleading-indentation -Wno-reorder -Wno-unused-variable -Wno-unused-value -Wno-overloaded-virtual -Wno-sign-compare
    -Wno-address -Wno-nonnull-compare -Wno-unused-but-set-variable -Wno-mismatched-new-delete -Wno-int-in-bool-context -Wno-comment
  )
  add_compile_options(${DREAMCAST_CFLAGS} ${DREAMCAST_WFLAGS})
  add_link_options(${DREAMCAST_CFLAGS} ${DREAMCAST_WFLAGS})
  add_definitions(-DPLATFORM_DREAMCAST)
  if(DREAMCAST_USE_FATFS)
    add_definitions(-DDREAMCAST_USE_FATFS)
  endif()
  if(DREAMCAST_USE_FILE_POOL)
    add_definitions(-DDREAMCAST_USE_FILE_POOL)
    add_link_options(-Wl,--wrap=fopen -Wl,--wrap=fclose -Wl,--wrap=fread -Wl,--wrap=fwrite -Wl,--wrap=fseek -Wl,--wrap=ftell -Wl,--wrap=setvbuf)
  endif()
  include_directories(
    ${KOS_BASE}/include
    ${KOS_BASE}/kernel/arch/dreamcast/include
    ${KOS_BASE}/addons/include
    ${KOS_BASE}/../kos-ports/include
  )
  link_directories(
    ${KOS_BASE}/lib/dreamcast
    ${KOS_BASE}/addons/lib/dreamcast
    ${KOS_BASE}/../kos-ports/lib
  )
endif()

if(TARGET_IS_BIG_ENDIAN)
  add_definitions(-DPLATFORM_BIG_ENDIAN)
else()
  add_definitions(-DPLATFORM_LITTLE_ENDIAN)
endif()

if(TARGET_IS_WINDOWS)
  add_definitions(-DPLATFORM_WIN32)
  add_definitions(-DWIN32)
  add_definitions(-DWINDOWS_IGNORE_PACKING_MISMATCH)
else()
  # TODO
  add_definitions(-DPLATFORM_POSIX)
  if(NOT TARGET_IS_DREAMCAST)
    add_definitions(-DPLATFORM_CASE_SENSITIVE_FS)
  endif()
  if(TARGET_IS_DREAMCAST)
    set(CMAKE_EXECUTABLE_SUFFIX ".elf")
  endif()
endif()

if(USE_SDL)
  add_definitions(-DPLATFORM_SDL)
endif()

if(LOW_MEMORY)
  add_definitions(-DPLATFORM_LOW_MEMORY)
endif()

if(MSVC)
  message(STATUS "Building ${CMAKE_BUILD_TYPE} with MSVC")
  add_definitions(-DPLATFORM_MSVC)
  add_compile_options(/nologo /fp:precise /analyze- /MP /MD /Gd /Gy /GS /Gm- /EHsc /Zi /Zc:wchar_t /Zc:inline /Zp4)
  add_compile_options(/wd4291 /wd4297)
  if(TARGET_IS_64BIT)
    set(MSVC_LIBDIR "Win64")
  else()
    set(MSVC_LIBDIR "Win32")
  endif()
else()
  message(STATUS "Building ${CMAKE_BUILD_TYPE} with GCC or Clang")
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Og -g)
    add_link_options(-g)
  elseif(LOW_MEMORY)
    add_compile_options(-Os)
  else()
    add_compile_options(-O2)
  endif()
  add_compile_options(-ffunction-sections -fno-strict-aliasing)
  add_compile_options(-Wno-write-strings -Wno-terminate)
endif()

# Dependencies

if(USE_SDL)
  if(MSVC)
    if(NOT DEFINED SDL2_INCLUDE_DIR)
      set(SDL2_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../Thirdparty/SDL2/include")
    endif()
    if(NOT DEFINED SDL2_LIBRARY)
      set(SDL2_LIBRARY "${CMAKE_SOURCE_DIR}/../Thirdparty/SDL2/lib/${MSVC_LIBDIR}/SDL2.lib")
    endif()
  else()
    # Won't bother with find_package as we're probably cross compiling.
    if(NOT DEFINED SDL2_INCLUDE_DIR)
      set(SDL2_INCLUDE_DIR )
    endif()
    if(NOT DEFINED SDL2_LIBRARY)
      set(SDL2_LIBRARY "-lSDL2")
    endif()
  endif()
endif()


# Subprojects

set(INSTALL_TARGETS Core Engine Render Audio IpDrv Fire)

set(CMAKE_SHARED_LIBRARY_PREFIX "")

add_subdirectory(Core)
add_subdirectory(Engine)
add_subdirectory(Render)
add_subdirectory(Audio)
add_subdirectory(IpDrv)
add_subdirectory(Fire)
#add_subdirectory(UCC)

if(USE_SDL)
  # SDL2
  add_subdirectory(NSDLDrv)
  list(APPEND INSTALL_TARGETS NSDLDrv)
else(TARGET_IS_DREAMCAST)
  # native Dreamcast driver
  add_subdirectory(KOSDrv)
  list(APPEND INSTALL_TARGETS KOSDrv)
endif()

if(BUILD_NOPENGLDRV)
  add_subdirectory(OpenGLDrv)
  list(APPEND INSTALL_TARGETS OpenGLDrv)
endif()

if(NOT PLATFORM_DREAMCAST)
  add_subdirectory(NOpenGLESDrv)
  list(APPEND INSTALL_TARGETS NOpenGLESDrv)
endif()

if(BUILD_PVRDRV)
  add_subdirectory(PVRDrv)
  list(APPEND INSTALL_TARGETS PVRDrv)
endif()

if(BUILD_NULLSOUNDDRV)
  add_subdirectory(SoundDrv)
  list(APPEND INSTALL_TARGETS SoundDrv)
endif()

if(BUILD_AICADRV)
  add_subdirectory(AICADrv)
  list(APPEND INSTALL_TARGETS AICADrv)
endif()

if(BUILD_EDITOR)
  # GUI requires WinDrv
  add_subdirectory(Editor)
  list(APPEND INSTALL_TARGETS Editor)
endif()


if(BUILD_UNREALTOURNAMENT)
  add_subdirectory(UnrealTournament)
  list(APPEND INSTALL_TARGETS UnrealTournament)
endif()

# assemble all modules and pdbs in one dir

if(BUILD_DCUTIL)
  set(BUILD_EDITOR ON)
  list(APPEND INSTALL_TARGETS DCUtil)
  add_subdirectory(DCUtil)
endif()

set(INSTALL_FILES )
set(INSTALL_PROGRAMS )

foreach(TGT ${INSTALL_TARGETS})
  get_target_property(TGT_TYPE ${TGT} TYPE)
  if(TGT_TYPE STREQUAL "EXECUTABLE")
    list(APPEND INSTALL_PROGRAMS "$<TARGET_FILE:${TGT}>")
  else()
    list(APPEND INSTALL_FILES "$<TARGET_FILE:${TGT}>")
  endif()
  if(MSVC)
    list(APPEND INSTALL_FILES "$<TARGET_PDB_FILE:${TGT}>")
  endif()
endforeach()

# Install executables with executable permissions preserved
if(INSTALL_PROGRAMS)
  install(PROGRAMS ${INSTALL_PROGRAMS} DESTINATION "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}")
endif()

# Install libraries and other files
if(INSTALL_FILES)
  install(FILES ${INSTALL_FILES} DESTINATION "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}")
endif()

# Dreamcast CDI image generation
if(TARGET_IS_DREAMCAST AND DREAMCAST_BUILD_CDI)
  find_program(MKDCDISC mkdcdisc)
  if(NOT MKDCDISC)
    message(FATAL_ERROR "mkdcdisc not found in PATH but DREAMCAST_BUILD_CDI=ON")
  endif()
  if(NOT DREAMCAST_GAME_DATA OR NOT EXISTS "${DREAMCAST_GAME_DATA}")
    message(FATAL_ERROR "DREAMCAST_GAME_DATA must point to the Unreal data directory when DREAMCAST_BUILD_CDI=ON")
  endif()
  if(NOT DREAMCAST_IP_TXT OR NOT EXISTS "${DREAMCAST_IP_TXT}")
    message(FATAL_ERROR "DREAMCAST_IP_TXT must point to ip.txt when DREAMCAST_BUILD_CDI=ON")
  endif()
  if(NOT DREAMCAST_IP_BIN)
    message(FATAL_ERROR "DREAMCAST_IP_BIN must not be empty when DREAMCAST_BUILD_CDI=ON")
  endif()

  if(NOT KOS_BASE)
    message(FATAL_ERROR "KOS_BASE is not set; required to locate makeip")
  endif()
  set(MAKEIP_EXE "${KOS_BASE}/utils/makeip/makeip")
  if(NOT EXISTS "${MAKEIP_EXE}")
    message(FATAL_ERROR "makeip not found at ${MAKEIP_EXE}")
  endif()

  get_filename_component(DREAMCAST_IP_BIN_DIR "${DREAMCAST_IP_BIN}" DIRECTORY)

  add_custom_command(OUTPUT "${DREAMCAST_IP_BIN}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DREAMCAST_IP_BIN_DIR}"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${DREAMCAST_IP_BIN}"
    COMMAND ${MAKEIP_EXE} "${DREAMCAST_IP_TXT}" "${DREAMCAST_IP_BIN}"
    DEPENDS "${DREAMCAST_IP_TXT}"
    COMMENT "Generating IP.BIN..."
    VERBATIM
  )

  add_custom_target(ip_bin ALL
    DEPENDS "${DREAMCAST_IP_BIN}"
  )

  add_custom_target(cdi
    COMMAND ${CMAKE_COMMAND} -E echo "Creating CDI image..."
    COMMAND ${MKDCDISC} -e "${DREAMCAST_ELF_FILE}" -D "${DREAMCAST_GAME_DATA}" -p "${DREAMCAST_IP_BIN}" -N -o "${DREAMCAST_CDI_OUTPUT}"
    DEPENDS UnrealTournament ip_bin
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    VERBATIM
  )
endif()