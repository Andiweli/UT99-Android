project(DCUtil)

set(SRC_FILES
  "Src/Main.cpp"
  "Src/Texture.cpp"
  "Src/Sound.cpp"
  "Src/Mesh.cpp"
  "Src/Model.cpp"
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

if(BUILD_STATIC)
  set(LINK_PACKAGES ${INSTALL_TARGETS})
  list(REMOVE_ITEM LINK_PACKAGES ${PROJECT_NAME})
  list(REMOVE_ITEM LINK_PACKAGES UnrealTournament)
  target_link_libraries(${PROJECT_NAME} -Wl,--whole-archive ${LINK_PACKAGES} -Wl,--no-whole-archive)
else()
  target_link_libraries(${PROJECT_NAME} Engine Core Editor)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE DCUTIL_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})
