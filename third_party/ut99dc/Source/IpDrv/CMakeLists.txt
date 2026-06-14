project(IpDrv CXX)

set(SRC_FILES
  "Src/TcpLink.cpp"
  "Src/UdpLink.cpp"
  "Src/IpDrv.cpp"
  "Src/UnSocket.cpp"
  "Src/InternetLink.cpp"
  "Src/TcpNetDriver.cpp"
  "Src/UMasterServerCommandlet.cpp"
  "Src/UUpdateServerCommandlet.cpp"

)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_link_libraries(${PROJECT_NAME} Engine Core)

if(TARGET_IS_DREAMCAST)
  target_link_libraries(${PROJECT_NAME} pthread)
endif()

if(TARGET_IS_WINDOWS)
  target_link_libraries(${PROJECT_NAME} ws2_32)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE IPDRV_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})
