cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
project(ChakravyuhaPasses CXX)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVM_DIR: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
add_compile_options(${LLVM_CXX_FLAGS})

add_library(ChakravyuhaStringEncryptionPass SHARED StringEncryptionPass.cpp)
target_link_libraries(ChakravyuhaStringEncryptionPass PRIVATE ${LLVM_LIBS})

set_target_properties(ChakravyuhaStringEncryptionPass PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
