# CMake file for mylapspoints.
cmake_minimum_required(VERSION 3.3)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/contrib/CMakePCHCompiler)

project(auto-pch CXX CXXPCH)


###############################################################################
#
# Find Boost
#
###############################################################################
set(Boost_USE_MULTI_THREADED ON)
set(Boost_USE_STATIC_LIBS ON)
find_package( Boost )
    # REQUIRED
    # unit_test_framework
    # system
    # regex
    # date_time
    # thread
    # filesystem
    # program_options
    # chrono
    # atomic)


###############################################################################
#
# Build auto-pch
#
###############################################################################

set(CMAKE_CXX_STANDARD 14)

# Find all of the .cpp and .h files in our paths
file(GLOB_RECURSE PROJECT_SOURCES src/*.cpp)
file(GLOB_RECURSE PROJECT_HEADERS src/*.hpp)

add_executable( auto-pch ${PROJECT_SOURCES} ${PROJECT_HEADERS} )

target_include_directories( 
  auto-pch PUBLIC "src" ${PROJECT_BINARY_DIR})

target_include_directories( 
  auto-pch SYSTEM PUBLIC
  ${Boost_INCLUDE_DIRS})

target_link_libraries(
  auto-pch 
  ${Boost_LIBRARIES})

target_precompiled_header(auto-pch pch/auto-pch.pch.hpp)

###############################################################################
#
# Check if we want to look for headers
#
###############################################################################
option( CHECK_INCLUDES_VIA_IWYU "Use include-what-you-use to verify include files." OFF)
if(CHECK_INCLUDES_VIA_IWYU)
  find_program(IWYU_PATH NAMES include-what-you-use iwyu)
  if(IWYU_PATH)
    set(IWYU_PATH_AND_OPTIONS ${IWYU_PATH}
      -Xiwyu --mapping_file=gcc.stl.headers.imp
      -Xiwyu --mapping_file=boost-all.imp
      -Xiwyu --mapping_file=boost-all-private.imp)
    set_property(TARGET mylapspoints PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_PATH_AND_OPTIONS})
  endif()
endif()



