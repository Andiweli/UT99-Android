project(Audio CXX)

set(SRC_FILES
  "Src/Audio.cpp"
  "Src/AudioCoreLinux.cpp"
  "Src/AudioLibrary.cpp"
  "Src/AudioMixer.cpp"
  "Src/AudioSubsystem.cpp"
  "Src/FormatWAV.cpp"
)
add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_link_libraries(${PROJECT_NAME} Engine Render Core ${CMAKE_DL_LIBS})

if(ANDROID)
  target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARY})

  # UT99_ANDROID_UMX_LIBXMP_V4_BEGIN
  # libxmp is imported by app/src/main/cpp/CMakeLists.txt when
  # app/src/main/jniLibs/${ANDROID_ABI}/libxmp.so exists.
  if(TARGET ut99_libxmp)
    target_include_directories(${PROJECT_NAME} PRIVATE ${LIBXMP_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ut99_libxmp)
    target_compile_definitions(${PROJECT_NAME} PRIVATE GENERIC_AUDIO_DIRECT_LIBXMP=1)
  endif()
  # UT99_ANDROID_UMX_LIBXMP_V4_END
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE AUDIO_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})

