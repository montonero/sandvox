project(sandvox)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_OSX_ARCHITECTURES x86_64)
set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++1y")
set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs")

set(BUILD_CPU_DEMOS OFF CACHE BOOL "Build original Bullet CPU demos")
set(BUILD_OBSOLETE_DEMOS OFF CACHE BOOL "Set when you want to build the obsolete Bullet 2 demos")
set(BUILD_BULLET3_DEMOS OFF CACHE BOOL "Set when you want to build the Bullet 3 demos")
set(BUILD_UNIT_TESTS OFF CACHE BOOL "Build Unit Tests")

add_subdirectory(deps/glfw)
include_directories(deps/glfw/include)
add_definitions(-DGLFW_INCLUDE_NONE)

include_directories(deps/glm)
add_definitions(-DGLM_FORCE_RADIANS)

add_subdirectory(deps/bullet3)
include_directories(deps/bullet3/src)

include_directories(src)

file(GLOB_RECURSE SOURCES src/*)

add_executable(sandvox ${SOURCES})

target_link_libraries(sandvox glfw)
target_link_libraries(sandvox BulletCollision BulletDynamics LinearMath)
target_link_libraries(sandvox "-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo")

SET_TARGET_PROPERTIES(sandvox PROPERTIES XCODE_ATTRIBUTE_GCC_PRECOMPILE_PREFIX_HEADER YES) 
SET_TARGET_PROPERTIES(sandvox PROPERTIES XCODE_ATTRIBUTE_GCC_PREFIX_HEADER src/common.hpp)
