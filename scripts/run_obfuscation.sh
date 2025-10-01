#!/bin/bash
# Exit immediately if any command fails
set -e

# --- Configuration ---
# The build directory should be the one created by CMake
BUILD_DIR=./build
SRC_FILE=./test_program.c

# Define output file paths
PASS_LIB=${BUILD_DIR}/lib/ChakravyuhaStringEncryptionPass.so
LLVM_IR=${BUILD_DIR}/test_program.ll
OBFUSCATED_IR=${BUILD_DIR}/obfuscated.ll
FINAL_BINARY=${BUILD_DIR}/obfuscated_program
REPORT_FILE=${BUILD_DIR}/report.json

# --- Check for prerequisites ---
if [ ! -f "$PASS_LIB" ]; then
    echo "Error: Pass library not found at $PASS_LIB"
    echo "Please build the project first with CMake."
    exit 1
fi

echo "--- [1/4] Compiling C to LLVM IR ---"
clang -O0 -S -emit-llvm "$SRC_FILE" -o "$LLVM_IR"

echo "--- [2/4] Running Obfuscation Pass ---"
# Note: Stderr (where the report is printed) is redirected to the report file
opt -load-pass-plugin="$PASS_LIB" -passes=chakravyuha-string-encrypt \
    "$LLVM_IR" -S -o "$OBFUSCATED_IR" 2>"$REPORT_FILE"

echo "--- [3/4] Compiling Obfuscated IR to Executable ---"
clang "$OBFUSCATED_IR" -o "$FINAL_BINARY"

echo "--- [4/4] Running Obfuscated Program ---"
echo "Output:"
"$FINAL_BINARY"

echo ""
echo "--- Verification ---"
echo "Running 'strings' on the binary. The secret string should NOT be visible:"
strings "$FINAL_BINARY" | grep "TEAM_CHAKRAVYUHA" || echo "(String not found, success!)"
echo ""
echo "Obfuscation complete. Final binary is at $FINAL_BINARY"
echo "Report generated at $REPORT_FILE"
