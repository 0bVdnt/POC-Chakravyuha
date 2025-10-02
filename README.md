# Chakravyuha: An Intelligent C/C++ Obfuscation Engine

[![License: AGPL v3](https://img.shields.io/badge/License-AGPL%20v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/0bvdnt/poc-chakravyuha)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-orange.svg)](https://github.com/0bvdnt/poc-chakravyuha)

## Introduction

Chakravyuha is an obfuscation framework for C/C++ applications, designed to provide robust protection for software intellectual property (IP). The primary objective of this project is to mitigate threats from reverse engineering, unauthorized modification, and software piracy. By leveraging the LLVM compiler infrastructure, Chakravyuha implements a series of code transformations that obscure program logic and conceal sensitive data, thereby increasing the complexity and cost of static and dynamic analysis for adversaries.

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

### Prerequisites

A complete LLVM and Clang development environment is required to build and run the obfuscation pass.

- **CMake** (version 3.13 or newer)
- **A C++ compiler** (Clang, GCC, or MSVC)
- **LLVM and Clang** (A newer version (20.1.0 or above) is recommended)

---

#### On Linux (Debian/Ubuntu)

```bash
sudo apt-get update
sudo apt-get install cmake build-essential clang llvm-dev
```

---

#### On Windows

Setting up a C++ development environment on Windows requires a compiler toolchain in addition to LLVM and CMake. You have two primary options. **Option 1 is recommended for most users.**

##### **Option 1: Using MSYS2 and MinGW-w64 (Recommended)**

1.  **Install MSYS2**
    - Download and install MSYS2 from the official website: [msys2.org](https://www.msys2.org/).
    - After installation, open the MSYS2 terminal and update the core packages:
      ```bash
      pacman -Syu
      # Close the terminal and re-open if prompted, then run again
      pacman -Su
      ```

2.  **Install Development Tools**
    - In the MSYS2 terminal, install the complete Clang/LLVM toolchain and CMake. We recommend the `UCRT64` environment as it uses the modern, standards-compliant C runtime from Microsoft.
      ```bash
      # This is the recommended command
      pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-llvm mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
      ```
    - > **Note:** You must use the corresponding MSYS2 terminal for the environment you choose. The `UCRT64` packages require the **"MSYS2 UCRT64"** terminal. The older `MINGW64` environment (which uses the legacy `msvcrt.dll` runtime) may also work for this project but is not the recommended choice for modern development.

3.  **Verify the Installation**
    - From the Start Menu, open the **"MSYS2 UCRT64"** terminal (or the "MSYS2 MINGW64" terminal if you chose the older environment).
    - In this new terminal, verify that the tools are available by running `clang --version`, `opt --version`, and `cmake --version`.

##### **Option 2: Using Visual Studio Build Tools**

1.  **Install Visual Studio Build Tools**
    - Download the **Visual Studio Installer** from the [Visual Studio website](https://visualstudio.microsoft.com/downloads/).
    - Run the installer. You do not need the full Visual Studio IDE.
    - Go to the **"Workloads"** tab and select **"Desktop development with C++"**. This will automatically include the necessary C++ compiler, linker, and Windows SDK.
    - Click "Install".

2.  **Install LLVM**
    - Download the latest **LLVM official release for Windows** from the [LLVM GitHub Releases page](https://github.com/llvm/llvm-project/releases).
    - Look for the asset named `LLVM-x.y.z-win64.exe`.
    - Run the installer. During installation, it is **critical** that you check the box for **"Add LLVM to the system PATH for all users"**.

3.  **Verify the Installation**
    - Open the **"Developer Command Prompt for VS 2022"** (or your version).
    - In this special terminal, verify that the tools are available by running `clang --version`, `opt --version`, and `cmake --version`.


---

#### On macOS (via Homebrew)

```bash
brew install llvm cmake
```
_Note: Homebrew's LLVM is "keg-only." Follow the instructions provided by `brew info llvm` to configure your environment paths._

## Build Instructions

The build process is now unified across all platforms thanks to CMake.

```bash
# 1. Clone the project repository
git clone https://github.com/0bvdnt/poc-chakravyuha.git
cd poc-chakravyuha

# 2. Create and navigate to a build directory
mkdir build
cd build

# 3. Configure the project with CMake
#    On Windows, you may need to specify the generator, e.g.,
#    cmake -G "Ninja" ..
#    cmake -G "Visual Studio 17 2022" ..
cmake ..

# 4. Compile the LLVM pass
#    This generates the shared library (.so or .dll) in the build/lib/ directory.
cmake --build .
```

A successful compilation will produce the pass library (e.g., `build/lib/ChakravyuhaStringEncryptionPass.so` or `build/lib/ChakravyuhaStringEncryptionPass.dll`).

## Usage Instructions

After successfully building the pass, use the provided scripts from the **project's root directory** to apply the obfuscation to the sample program (`test_program.c`).

### On Linux or macOS

```bash
# First, make the script executable
chmod +x scripts/run_obfuscation.sh

# Run the full obfuscation pipeline
./scripts/run_obfuscation.sh

# or

./scripts/run_obfuscation.sh "string_to_find_in_the_binary"
```

### On Windows (Command Prompt or PowerShell)

```powershell
# Run the full obfuscation pipeline
.\scripts\run_obfuscation.bat

# or
.\scripts\run_obfuscation.bat "string_to_find_in_the_binary"
```
_Note: If no argument is provided, the default string "SUPER_SECRET_STRING" will be used._


These scripts automate the following sequence:
1.  **IR Generation**: Compiles `test_program.c` to LLVM Intermediate Representation.
2.  **Obfuscation Pass**: Executes the `opt` tool to load your compiled pass and apply it to the IR.
3.  **Binary Compilation**: Compiles the obfuscated LLVM IR into a native executable.
4.  **Execution**: Runs the final obfuscated binary to demonstrate its functional correctness.

### Verification of Results

After running the script, you can confirm the efficacy of the obfuscation:

1.  **Functional Test:** The script will automatically run the final binary. The expected output is `SUPER_SECRET_STRING`. This confirms that the runtime decryption works correctly.

2.  **Static Analysis:** The script will also run the `strings` utility on the final binary. The output should not contain the plaintext literal "SUPER_SECRET_STRING", confirming that the string is hidden.

3.  **Review the Obfuscation Report:** Examine the contents of `build/report.json` for a detailed summary of the applied transformations and their corresponding metrics.

## Development Roadmap

This Proof-of-Concept serves as the foundation for a more comprehensive obfuscation tool. Future work will focus on the implementation of the following modules:

- [ ] **Control Flow Flattening Pass**
- [ ] **Bogus Code Insertion Pass**
- [ ] **Anti-Debug and Anti-VM Instrumentation Pass**
- [ ] **A Heuristic-Based Pass Manager** for automated selection and sequencing of obfuscation passes.
- [x] **Support for the Windows PE file format**.
- [ ] **Implementation of dynamic and context-aware encryption keys**.

## License

This project is distributed under a dual-license model.

- **Community Edition**: Licensed under the GNU Affero General Public License v3.0 (AGPL-3.0). This is a strong copyleft license intended to foster community collaboration and ensure that derivative works remain open source. Please see the [LICENSE](LICENSE) file for the full terms.
- **Commercial License**: A separate commercial license is available for organizations that intend to integrate Chakravyuha into proprietary software products without being subject to the obligations of the AGPL-3.0. Please direct all commercial licensing inquiries to the project maintainers.