# Chakravyuha: An Intelligent C/C++ Obfuscation Engine

[![License: AGPL v3](https://img.shields.io/badge/License-AGPL%20v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/0bvdnt/poc-chakravyuha)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-orange.svg)](https://github.com/0bvdnt/poc-chakravyuha)

## Introduction

Chakravyuha is an advanced obfuscation framework for C/C++ applications, designed to provide robust protection for software intellectual property (IP). The primary objective of this project is to mitigate threats from reverse engineering, unauthorized modification, and software piracy. By leveraging the LLVM compiler infrastructure, Chakravyuha implements a series of code transformations that obscure program logic and conceal sensitive data, thereby increasing the complexity and cost of static and dynamic analysis for adversaries.

This repository contains a Proof-of-Concept (PoC) that demonstrates a core obfuscation technique: string literal encryption implemented as an LLVM pass. The framework is designed to be extensible, with a long-term vision of incorporating intelligent, adaptive obfuscation strategies.

---

## Technical Objectives and Features

The Chakravyuha engine is architected to evolve beyond static obfuscation patterns into an intelligent system that tailors its transformations based on code analysis.

- **Adaptive Obfuscation Strategy:** The system is designed to eventually incorporate a heuristic or model-driven pass manager that analyzes the Abstract Syntax Tree (AST) or LLVM Intermediate Representation (IR) to apply the most effective sequence of obfuscation techniques for a given code structure.

- **Control Flow Flattening:** This technique obscures the natural control flow of a program by transforming function bodies into a single dispatcher loop with a state variable, making the logical sequence of operations difficult to follow.

- **Bogus Code Insertion:** Involves the injection of semantically inert but computationally complex code blocks to mislead static analysis and increase the cognitive load on a human analyst.

- **String Literal Encryption:** Protects embedded constants such as API keys, credentials, and proprietary messages from being identified through static analysis of the binary. The PoC implements this feature via XOR encryption with a runtime decryption stub.

- **Anti-Analysis Defenses:** The architecture includes provisions for injecting environmental checks to detect and react to the presence of debuggers, virtual machines, and other analysis tools, thereby impeding dynamic analysis.

- **Quantitative Reporting:** The framework will generate structured JSON reports that provide detailed metrics on the applied transformations, offering a quantitative measure of the obfuscation's impact on the binary.

- **LLVM-Based Architecture:** The solution is built upon the LLVM compiler framework, ensuring a modular, maintainable, and robust design that can operate on a standardized Intermediate Representation.

## System Requirements and Setup

This section provides instructions for configuring the necessary development environment and compiling the LLVM pass.

### 1. Prerequisites

A complete LLVM and Clang development environment is required to build and run the obfuscation pass.

- **CMake** (version 3.13 or newer)
- **A C++ compiler** (GCC or Clang)
- **LLVM and Clang development libraries** (version 14 or newer is recommended)

**On Debian-based systems (e.g., Ubuntu):**

```bash
sudo apt-get update
sudo apt-get install cmake build-essential clang llvm-dev
```

_Note: If using a versioned package (e.g., `llvm-15-dev`), ensure the tool names in the `Makefile` are updated accordingly (e.g., `CLANG=clang-15`)._

**On macOS (via Homebrew):**

```bash
brew install llvm cmake
```

_Note: Homebrew's LLVM is "keg-only." Follow the instructions provided by `brew info llvm` to configure your environment paths._

### 2. Building the Obfuscation Pass

Clone the repository and use the standard CMake workflow to compile the shared library.

```bash
# 1. Clone the project repository
git clone https://github.com/0bvdnt/poc-chakravyuha.git
cd poc-chakravyuha

# 2. Create and navigate to a build directory
mkdir build
cd build

# 3. Configure the project with CMake
#    CMake will locate the LLVM installation.
cmake ..

# 4. Compile the LLVM pass
#    This generates the shared library in the build/lib/ directory.
make
```

A successful compilation will produce the pass library at `build/lib/ChakravyuhaStringEncryptionPass.so`.

## Usage Instructions

The included `Makefile`, located in the project's root directory, orchestrates the entire process of applying the obfuscation pass to the sample program (`test_program.c`).

### Executing the Obfuscation Pipeline

From the project's **root directory**, execute the `make` command.

```bash
make
```

This command automates the following sequence:

1.  **IR Generation**: Compiles `test_program.c` to LLVM Intermediate Representation (`test_program.ll`) using Clang.
2.  **Obfuscation Pass**: Executes the `opt` tool to load the compiled pass (`ChakravyuhaStringEncryptionPass.so`) and apply it to `test_program.ll`. This step generates the obfuscated IR (`obfuscated.ll`) and a `report.json` file.
3.  **Binary Compilation**: Compiles the obfuscated LLVM IR into a native executable file (`obfuscated_program`).
4.  **Execution**: Runs the final obfuscated binary to demonstrate its functional correctness.

### Verification of Results

To confirm the efficacy of the obfuscation, perform the following checks:

1.  **Functional Test:**

    ```bash
    ./obfuscated_program
    # Expected Output: TEAM_CHAKRAVYUHA
    ```

    The program's output should remain correct, as the encrypted string is decrypted at runtime before use.

2.  **Static Analysis:**

    ```bash
    make strings
    ```

    This command runs the `strings` utility on the final binary. The output should not contain the plaintext literal "TEAM_CHAKRAVYUHA", confirming that the string is not present in its original form in the binary.

3.  **Review the Obfuscation Report:**
    Examine the contents of `report.json` for a detailed summary of the applied transformations and their corresponding metrics.

### Additional Makefile Targets

- **`make clean`**: Deletes all generated artifacts, including IR files, binaries, and reports.
- **`make no_obfuscation`**: Compiles the test program without applying the obfuscation pass, creating an unprotected binary for comparison.

## Development Roadmap

This Proof-of-Concept serves as the foundation for a more comprehensive obfuscation tool. Future work will focus on the implementation of the following modules:

- [ ] **Control Flow Flattening Pass**
- [ ] **Bogus Code Insertion Pass**
- [ ] **Anti-Debug and Anti-VM Instrumentation Pass**
- [ ] **A Heuristic-Based Pass Manager** for automated selection and sequencing of obfuscation passes.
- [ ] **Support for the Windows PE file format**.
- [ ] **Implementation of dynamic and context-aware encryption keys**.

## License

This project is distributed under a dual-license model.

- **Community Edition**: Licensed under the GNU Affero General Public License v3.0 (AGPL-3.0). This is a strong copyleft license intended to foster community collaboration and ensure that derivative works remain open source. Please see the [LICENSE](LICENSE) file for the full terms.
- **Commercial License**: A separate commercial license is available for organizations that intend to integrate Chakravyuha into proprietary software products without being subject to the obligations of the AGPL-3.0. Please direct all commercial licensing inquiries to the project maintainers.
