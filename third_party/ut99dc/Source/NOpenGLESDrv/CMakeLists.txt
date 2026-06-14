project(NOpenGLESDrv C CXX)

if(NOT USE_SDL)
  message(FATAL_ERROR "NOpenGLESDrv requires NSDLDrv to function.")
endif()

if(ANDROID)
  set(ANDROID_GLAD_COMPAT_DIR "${CMAKE_SOURCE_DIR}/android_glad_compat")
  set(ANDROID_GLM_COMPAT_DIR "${CMAKE_SOURCE_DIR}/android_glm_compat")
  set(SRC_FILES "NOpenGLESDrv.cpp" "${ANDROID_GLAD_COMPAT_DIR}/glad.c")
  set(NOPENGLES_GLAD_INCLUDE_DIRS "${ANDROID_GLAD_COMPAT_DIR}")
  set(NOPENGLES_GLM_INCLUDE_DIRS "${ANDROID_GLM_COMPAT_DIR}")
else()
  set(SRC_FILES "NOpenGLESDrv.cpp" "${CMAKE_SOURCE_DIR}/../Thirdparty/glad_es/glad.c")
  set(NOPENGLES_GLAD_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/../Thirdparty/glad_es
    ${CMAKE_SOURCE_DIR}/../Thirdparty
  )
  set(NOPENGLES_GLM_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/../Thirdparty)
endif()

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE
  ${NOPENGLES_GLAD_INCLUDE_DIRS}
  ${NOPENGLES_GLM_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../Render/Src
)

target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARY})

if(ANDROID)
  target_link_libraries(${PROJECT_NAME} GLESv2 log android)
endif()

target_link_libraries(${PROJECT_NAME} Render Engine Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE NOPENGLESDRV_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})
