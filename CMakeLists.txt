cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
project(ChakravyuhaPasses CXX C)

set(LLVM_LINK_LLVM_DYLIB ON)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVM_DIR: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_library(ChakravyuhaStringEncryptionPass MODULE StringEncryptionPass.cpp)

target_link_libraries(ChakravyuhaStringEncryptionPass PRIVATE LLVM)

set_target_properties(ChakravyuhaStringEncryptionPass PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    PREFIX ""
)
