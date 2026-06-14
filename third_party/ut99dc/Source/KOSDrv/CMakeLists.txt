project(KOSDrv CXX)

if(NOT PLATFORM_DREAMCAST)
  message(FATAL_ERROR "KOSDrv is for Dreamcast only.")
endif()

set(SRC_FILES
  "Src/KOSViewport.cpp"
  "Src/KOSClient.cpp"
  "Src/KOSDrv.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
  ${KOS_BASE}/include
  ${KOS_BASE}/kernel/arch/dreamcast/include
)

target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARY} Engine Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE KOSDRV_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})
