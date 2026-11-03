find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

target_link_libraries(nekodrome PRIVATE CURL::libcurl Threads::Threads)
set_target_properties(nekodrome PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_custom_target(nekodrome_assets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${NEKOUI_DIR}/assets ${CMAKE_BINARY_DIR}/bin/data
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/assets/logo.png ${CMAKE_BINARY_DIR}/bin/data/logo.png)
add_dependencies(nekodrome nekodrome_assets)

include("${CMAKE_SOURCE_DIR}/target_deps/Packaging.cmake")
