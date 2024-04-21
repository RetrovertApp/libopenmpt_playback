cmake_minimum_required(VERSION 3.10)
include(${CMAKE_SOURCE_DIR}/../scripts/CMakeSettings.txt)

project(libopenmpt_playback VERSION 0.0.3 DESCRIPTION "libopenmpt playback library for Retrovert" LANGUAGES CXX)

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
add_library(libopenmpt_playback SHARED ${SOURCES} 
    "libopenmpt/libopenmpt/libopenmpt_c.cpp"
    "libopenmpt/libopenmpt/libopenmpt_cxx.cpp"
    "libopenmpt/libopenmpt/libopenmpt_impl.cpp"
    "libopenmpt/libopenmpt/libopenmpt_ext_impl.cpp"
    "libopenmpt_plugin.cpp")

set_custom_library_extension(libopenmpt_playback)

target_compile_definitions(libopenmpt_playback PRIVATE LIBOPENMPT_BUILD)
