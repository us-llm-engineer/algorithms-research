cmake_minimum_required(VERSION 3.25)
project(algorithms_research LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ALGO_SANITIZE "Build with AddressSanitizer and UBSan" OFF)
option(ALGO_BUILD_BENCH "Build the benchmark executables" OFF)

add_compile_options(-Wall -Wextra -Wpedantic -Werror)
if(ALGO_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

find_package(Catch2 3 CONFIG REQUIRED)
include(CTest)
include(Catch)
include(cmake/AlgoModule.cmake)

add_library(algo_common INTERFACE)
target_include_directories(algo_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/common)

add_executable(common_test common/tests/common_test.cpp)
target_link_libraries(common_test PRIVATE algo_common Catch2::Catch2WithMain)
catch_discover_tests(common_test)

# Every directory below modules/<domain>/<name>/ that has a CMakeLists.txt is a module.
file(GLOB ALGO_MODULE_LISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/*/*/CMakeLists.txt")
foreach(list_file IN LISTS ALGO_MODULE_LISTS)
  get_filename_component(module_dir "${list_file}" DIRECTORY)
  get_filename_component(module_name "${module_dir}" NAME)
  add_subdirectory("${module_dir}" "${CMAKE_CURRENT_BINARY_DIR}/mod_${module_name}")
endforeach()
