cmake_minimum_required(VERSION 3.10)
project(qualcomm_proto CXX)

# STATIC = .a on Linux/Android; does not require runtime loading.
# Both monitor_c and the decoder link against this at build time;
# the symbols are merged into the final .so or executable.
add_library(qualcomm_proto STATIC
    utils.cpp
    log_config.cpp
    consts.cpp
)

# PUBLIC means any target that links qualcomm_proto also gets this include path.
# So monitor_c and decoder can #include "consts.h" without setting -I themselves.
target_include_directories(qualcomm_proto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(qualcomm_proto PRIVATE
    -std=c++14
    -Wall
    # -fPIC is set automatically by the Android NDK for shared library targets;
    # it is also needed on Linux when linking into a .so (Python extension).
    -fPIC
)