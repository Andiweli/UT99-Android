project(Render CXX)

set(SRC_FILES
  "Src/UnLight.cpp"
  "Src/UnMeshRn.cpp"
  "Src/UnRandom.cpp"
  "Src/UnRender.cpp"
  "Src/UnSoftLn.cpp"
  "Src/UnSpan.cpp"
  "Src/UnSprite.cpp"
  "Src/Render.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

if(TARGET_IS_DREAMCAST AND NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"))
  # FIXME: the rest is built with -Os, but building this file with -Os causes lightmaps to break
  set_source_files_properties("Src/UnLight.cpp" PROPERTIES COMPILE_FLAGS -O2)
endif()

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_link_libraries(${PROJECT_NAME} Engine Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE RENDER_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})
