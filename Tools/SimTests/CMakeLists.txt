cmake_minimum_required(VERSION 3.20)
project(NachtwerkSimTests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NW_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../Source/NWCore)
file(GLOB NW_SIM_SOURCES CONFIGURE_DEPENDS ${NW_CORE_DIR}/Private/Sim/*.cpp)

add_executable(nw_sim_tests SimTests.cpp ${NW_SIM_SOURCES})
target_include_directories(nw_sim_tests PRIVATE ${NW_CORE_DIR}/Public)

# Strict warnings: the same code must also compile cleanly inside Unreal.
if(MSVC)
	target_compile_options(nw_sim_tests PRIVATE /W4 /WX /permissive-)
else()
	target_compile_options(nw_sim_tests PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)
endif()

enable_testing()
add_test(NAME nw_sim_tests COMMAND nw_sim_tests)
