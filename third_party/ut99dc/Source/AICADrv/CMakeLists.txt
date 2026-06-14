project(AICADrv C CXX)

set(SRC_FILES 
  "AICADrv.cpp"
  "AudioEngine.cpp"
  "AicaInterface.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE
)


target_link_libraries(${PROJECT_NAME} Engine Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE AICADRV_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})

