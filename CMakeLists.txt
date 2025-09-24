cmake_minimum_required(VERSION 3.13)
project(PocObfuscator)

# --- FIX 1: Enforce a modern C++ standard ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# ------------------------------------------

set(LLVM_DIR "/home/siddhesh/dev/llvm-project/build/lib/cmake/llvm/")

find_package(LLVM REQUIRED CONFIG)

# Add LLVM include directories and definitions
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Our pass is a shared library
add_library(StringEncryptPass MODULE src/StringEncryptPass.cpp)

# Link against the necessary LLVM components
llvm_map_components_to_libnames(llvm_libs support core irreader IR TransformsUtils)
target_link_libraries(StringEncryptPass PRIVATE ${llvm_libs})
