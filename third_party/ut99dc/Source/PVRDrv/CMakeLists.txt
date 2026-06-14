project(PVRDrv C CXX)

if(NOT TARGET_IS_DREAMCAST)
  message(FATAL_ERROR "PVRDrv is for the Dreamcast only.")
endif()

set(SRC_FILES "PVRDrv.cpp")

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
)

target_include_directories(${PROJECT_NAME}
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../Render/Src
  ${KOS_BASE}/include
)

target_include_directories(${PROJECT_NAME}
  PRIVATE
  ${KOS_BASE}/include
)

target_link_libraries(${PROJECT_NAME} Render Engine Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE GLDCDRV_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})
