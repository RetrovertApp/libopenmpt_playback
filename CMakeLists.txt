cmake_minimum_required(VERSION 3.10)
project(playback_libopenmpt VERSION 0.0.3 DESCRIPTION "libopenmpt playback library for Retrovert" LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Define sources
file(GLOB_RECURSE SOURCES 
    "libopenmpt/soundlib/*.cpp"
    "libopenmpt/common/*.cpp"
    "libopenmpt/sounddsp/*.cpp")

# Define headers
include_directories(
    "retrovert_api/c" 
    "libopenmpt" 
    "libopenmpt/src" 
    "libopenmpt/include" 
    "libopenmpt/soundlib" 
    "libopenmpt/common" 
    "libopenmpt/sounddsp")

# Create shared library
add_library(playback_libopenmpt SHARED ${SOURCES} 
    "libopenmpt/libopenmpt/libopenmpt_c.cpp"
    "libopenmpt/libopenmpt/libopenmpt_cxx.cpp"
    "libopenmpt/libopenmpt/libopenmpt_impl.cpp"
    "libopenmpt/libopenmpt/libopenmpt_ext_impl.cpp"
    "libopenmpt_plugin.cpp")

target_compile_definitions(playback_libopenmpt PRIVATE LIBOPENMPT_BUILD)
