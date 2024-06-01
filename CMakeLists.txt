cmake_minimum_required(VERSION 3.18)

project(gpn_tron VERSION 0.1.0)

include(cmake_helpers/BuildOptions.cmake)
include(deps.cmake)

add_executable(gpn_tron src/main.cpp)

target_add_default_build_options(gpn_tron PUBLIC)

target_link_libraries(
  gpn_tron PUBLIC Threads::Threads fmt::fmt ${boost_targets})
