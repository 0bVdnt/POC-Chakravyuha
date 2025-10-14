# Chakravyuha: An Intelligent C/C++ Obfuscation Engine

[![Top Language](https://img.shields.io/github/languages/top/0bvdnt/poc-chakravyuha?style=for-the-badge&color=blue)](https://github.com/0bvdnt/poc-chakravyuha)
[![LLVM Version](https://img.shields.io/badge/LLVM-20.1+-blueviolet?style=for-the-badge&logo=llvm)](https://llvm.org/)
[![Last Commit](https://img.shields.io/github/last-commit/0bvdnt/poc-chakravyuha?style=for-the-badge&color=brightgreen)](https://github.com/0bvdnt/poc-chakravyuha/commits/main)
[![Repository Size](https://img.shields.io/github/repo-size/0bvdnt/poc-chakravyuha?style=for-the-badge)](https://github.com/0bvdnt/poc-chakravyuha)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPL%20v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/0bvdnt/poc-chakravyuha)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-orange.svg)](https://github.com/0bVdnt/LLVM-Passes/tree/main/ChakraPasses)

## 1. Introduction

Chakravyuha is a powerful suite of code obfuscation passes for the LLVM compiler infrastructure. It is engineered as a cross-platform tool that transforms C/C++ source code at the Intermediate Representation (IR) level to produce executables that are significantly more resistant to reverse engineering, static analysis, and tampering.

By integrating directly into the modern LLVM toolchain, Chakravyuha provides a robust and modular framework for protecting software intellectual property (IP), securing sensitive algorithms, and hardening applications against unauthorized analysis.

---

## 2. Key Features & Capabilities

Chakravyuha's strength lies in its layered, multi-faceted approach to obfuscation. Each pass is designed to counter specific reverse engineering techniques, and when combined, they create a formidable defensive barrier.

### Advanced String Encryption

This is not a simple XOR cipher. The `chakravyuha-string-encrypt` pass implements a highly resilient, multi-layered scheme to protect sensitive embedded strings like API keys, credentials, and proprietary messages.

- **Polymorphic Encryption Engine:** At compile time, the pass randomly selects one of several different encryption ciphers (`XOR`, `ADD`, `SUB`, `S-Box Substitution`) for each individual string. This polymorphism means that even if an attacker breaks the encryption for one string, that knowledge cannot be used to decrypt others.
- **Per-String Static Keys:** A unique, cryptographically random key is generated for every single string. This key is then cleverly obfuscated and embedded within its corresponding decryption stub. This architecture eliminates the risk of a single "master key" being discovered and used to compromise all secrets.
- **Just-in-Time, On-Demand Decryption:** To defeat memory scanners and simple dynamic analysis, strings are **not** decrypted all at once. A unique decryption routine is executed only the very first time a string is accessed during runtime. Before that, the string exists only in its encrypted form in memory.
- **Self-Modifying Pointers for Performance:** The on-demand decryption is achieved with minimal performance overhead. Access to a string is replaced by a call through a function pointer. Initially, this pointer calls a "slow" function that decrypts the string and then **atomically overwrites the pointer** to a new "fast" function. All subsequent calls go directly to the fast function, which simply returns the already-decrypted string, making the one-time decryption cost negligible.

### Control Flow Flattening

The `chakravyuha-control-flow-flatten` pass is designed to completely dismantle the logical structure of a function, making it incredibly difficult for a human or a decompiler to understand.

- **State Machine Transformation:** The pass deconstructs a function's natural control flow (e.g., `if`/`else` branches, `for`/`while` loops) and rebuilds it as a large, flat state machine. All the original code blocks are placed as cases within a central `switch` statement inside a dispatcher loop.
- **State Variable Control:** Instead of direct jumps and calls, the program's execution path is determined by a state variable. At the end of each logical block, this variable is updated to point to the next block, and control returns to the central dispatcher. This transforms a clear, readable Control Flow Graph (CFG) into a "spaghetti-like" structure that is bewildering to analyze.
- **Robust and Safe:** The pass is engineered for stability. It automatically detects and skips functions that contain constructs incompatible with flattening (like inline assembly or `setjmp`/`longjmp`), ensuring that the obfuscation process does not break the build or introduce runtime errors.

### Fake Code Insertion

To further confuse and mislead reverse engineers, the `chakravyuha-fake-code-insertion` pass pollutes the binary with deceptive but non-functional code.

- **Opaque Predicates:** The pass injects junk code blocks using conditional branches that are guaranteed to never be taken at runtime. These "opaque predicates" (e.g., `if (x != x)`) are difficult for static analysis tools to prove as dead code, forcing them to analyze the junk blocks as if they were part of the real program logic.
- **Computationally Complex Junk:** The inserted blocks are not empty. They are filled with a series of computationally intensive but ultimately meaningless arithmetic and bitwise operations. This creates the illusion of complex algorithms, wasting an analyst's time and drawing their attention away from the code that actually matters.

### Quantitative and Visual Reporting

To measure the effectiveness of the obfuscations, Chakravyuha provides a powerful reporting framework.

- **Detailed JSON Metrics:** A structured JSON file is generated, providing hard data on the applied transformations. This includes metrics like the number of functions flattened, fake blocks inserted, strings encrypted, and the percentage change in both IR data and final binary size.
- **Interactive HTML Visual Report:** A dynamic `index.html` report is created to provide a clear, side-by-side visual comparison of a function's Control Flow Graph (CFG) before and after obfuscation. This allows developers to instantly see and appreciate the structural complexity introduced by the flattening pass.

---

## 3. Prerequisites & Installation

Before you begin, ensure you have the required development tools for your platform.

### Windows (using MSYS2)

Using the [MSYS2](https://www.msys2.org/) environment is recommended. After installing, open the **UCRT64** terminal and run:

```bash
# 1. Update package database and base packages
pacman -Syu

# 2. Install required development toolchain and libraries
pacman -S --needed \
    make git python3 \
    mingw-w-ucrt-x86_64-toolchain \
    mingw-w-ucrt-x86_64-cmake \
    mingw-w-ucrt-x86_64-llvm \
    mingw-w-ucrt-x86_64-graphviz
```

### Ubuntu / Debian

```bash
sudo apt-get update && sudo apt-get install -y \
    build-essential cmake git python3 llvm clang graphviz
```

### macOS (using Homebrew)

```bash
# 1. Install Homebrew if you haven't already
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 2. Install required packages
brew install cmake git python llvm graphviz
```

---

## 4. Build Instructions

The project uses CMake for a unified, cross-platform build process.

1.  **Clone the repository:**

    ```bash
    git clone https://github.com/0bvdnt/poc-chakravyuha.git
    cd poc-chakravyuha
    ```

2.  **Create and enter a build directory:**

    ```bash
    mkdir build && cd build
    ```

3.  **Configure the project with CMake:**
    - **On Linux/MSYS2:**
      ```bash
      cmake ..
      ```
    - **On macOS (with Homebrew LLVM):**
      ```bash
      cmake -D LLVM_DIR=$(brew --prefix llvm)/lib/cmake/llvm ..
      ```

4.  **Compile the pass library:**
    `bash
cmake --build . --config Release
`
    After a successful build, the pass library (`ChakravyuhaPasses.dll`, `.so`, or `.dylib`) will be located in the `build/lib/` directory.

---

## 5. Usage Instructions

All testing and visualization is handled by two cross-platform Python scripts.

### Step 1: Run the Test Suite

This script automatically compiles, obfuscates, and verifies all test cases, providing a comprehensive functional check.

- **To run the full suite of obfuscations (recommended):**
  ```bash
  python3 run_tests.py --pipeline full
  ```
- **To run only a specific pass (e.g., `cff` for control flow flattening):**
  ```bash
  python3 run_tests.py --pipeline cff
  ```
  (Available pipelines: `full`, `cff`, `string`, `fake`).

### Step 2: Visualize the Results

After running the tests, generate an interactive HTML report to see the results.

- **To generate the report and open it in your browser:**
  ```bash
  python3 create_comparison.py view
  ```

This will create the report at `test_results/visualizations/comparison/index.html`.

---

## 6. Development Roadmap

- [x] **Control Flow Flattening Pass**
- [x] **Bogus Code Insertion Pass**
- [x] **Advanced String Encryption Pass** (Polymorphic, Per-String Static Keys)
- [x] **Cross-Platform Support (Windows, Linux, macOS)**
- [x] **Quantitative & Visual Reporting Framework**
- [ ] **Anti-Debug and Anti-VM Instrumentation Pass**
- [ ] **A Heuristic-Based Pass Manager** for automated selection and sequencing of obfuscation passes.
- [ ] **Implementation of dynamic and context-aware encryption keys** (e.g., keys derived from the runtime environment).

---

## 7. License

This project is distributed under a dual-license model.

- **Community Edition**: Licensed under the GNU Affero General Public License v3.0 (AGPL-3.0). This is a strong copyleft license intended to foster community collaboration and ensure that derivative works remain open source. Please see the [LICENSE](LICENSE) file for the full terms.
- **Commercial License**: A separate commercial license is available for organizations that intend to integrate Chakravyuha into proprietary software products without being subject to the obligations of the AGPL-3.0. Please direct all commercial licensing inquiries to the project maintainers.
