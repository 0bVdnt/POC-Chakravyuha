# CMakeLists.txt
cmake_minimum_required(VERSION 3.13)
project(PocObfuscator)

# IMPORTANT: Point this to your LLVM build directory
set(LLVM_DIR "/home/siddhesh/dev/llvm-project/build/lib/cmake/llvm/") 

find_package(LLVM REQUIRED CONFIG)

# Add LLVM include directories
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Our pass is a shared library
add_library(StringEncryptionPass MODULE src/StringEncryptionPass.cpp)

# Link against the necessary LLVM components
llvm_map_components_to_libnames(llvm_libs support core irreader)
target_link_libraries(StringEncryptionPass ${llvm_libs})
