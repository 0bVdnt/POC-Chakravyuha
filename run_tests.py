#!/usr/bin/env python3
import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

# --- Configuration ---
SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR
BUILD_DIR = PROJECT_ROOT / "build"
TEST_SRC_DIR = SCRIPT_DIR / "tests"
RESULTS_DIR = SCRIPT_DIR / "test_results"

# --- Pass Pipelines ---
PASS_PIPELINES = {
    "full": (
        "chakravyuha-string-encrypt,"
        "chakravyuha-control-flow-flatten,"
        "chakravyuha-fake-code-insertion"
    ),
    "cff": "chakravyuha-control-flow-flatten",
    "string": "chakravyuha-string-encrypt",
    "fake": "chakravyuha-fake-code-insertion",
}

# --- Terminal Colors ---


class Colors:
    GREEN = "\033[0;32m"
    RED = "\033[0;31m"
    YELLOW = "\033[1;33m"
    CYAN = "\033[0;36m"
    NC = "\033[0m"


# --- Helper Functions ---


def find_exec(name, msg):
    exec_path = shutil.which(name)
    if not exec_path:
        print(f"{Colors.RED}Error: {msg}{Colors.NC}", file=sys.stderr)
        sys.exit(1)
    return Path(exec_path)


def is_wsl():
    """Check if the script is running inside WSL."""
    return "microsoft" in platform.uname().release.lower()


def find_pass_plugin():
    system = platform.system()
    if system == "Windows":
        plugin_name = "ChakravyuhaPasses.dll"
    elif system == "Darwin":
        plugin_name = "ChakravyuhaPasses.dylib"
    else:
        plugin_name = "ChakravyuhaPasses.so"

    search_paths = [
        BUILD_DIR / "lib" / plugin_name,
        BUILD_DIR / plugin_name,
        BUILD_DIR / "Debug" / plugin_name,
        BUILD_DIR / "Release" / plugin_name,
    ]
    for path in search_paths:
        if path.exists():
            print(f"{Colors.GREEN}Found pass plugin at: {path}{Colors.NC}")
            return path
    print(
        f"{Colors.RED}Error: Could not find '{plugin_name}'.{Colors.NC}",
        file=sys.stderr,
    )
    print("Please build the project first using CMake.", file=sys.stderr)
    sys.exit(1)


def run_command(cmd_args, log_file=None, env=None):
    try:
        cmd_args_str = [str(arg) for arg in cmd_args]
        process_kwargs = {"check": True, "text": True, "env": env}
        if log_file:
            with open(log_file, "w") as f:
                subprocess.run(
                    cmd_args_str, stdout=f, stderr=subprocess.STDOUT, **process_kwargs
                )
        else:
            subprocess.run(
                cmd_args_str,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                **process_kwargs,
            )
        return True, ""
    except subprocess.CalledProcessError as e:
        error_output = (
            e.stdout + e.stderr
            if e.stdout and e.stderr
            else e.stdout or e.stderr or "No output."
        )
        return False, error_output
    except FileNotFoundError:
        return False, f"Command not found: {cmd_args[0]}"


# --- Main Test Logic ---


def run_test(test_c_file, pipeline_name, pass_plugin_path, clang, opt, env):
    test_name = test_c_file.stem
    print(
        f"\n{Colors.YELLOW}--- Testing: {test_name} (Pipeline: {pipeline_name}) ---{
            Colors.NC
        }"
    )

    exe_suffix = ".exe" if platform.system() == "Windows" else ""
    original_bin = RESULTS_DIR / "binaries" / f"{test_name}_original{exe_suffix}"
    obfuscated_bin = (
        RESULTS_DIR / "binaries" / f"{test_name}_{pipeline_name}{exe_suffix}"
    )
    original_ll, obfuscated_ll = (
        RESULTS_DIR / "ll_files" / f"{test_name}.ll",
        RESULTS_DIR / "ll_files" / f"{test_name}_{pipeline_name}.ll",
    )
    report_json, log_file = (
        RESULTS_DIR / "reports" / f"{test_name}_{pipeline_name}.json",
        RESULTS_DIR / "logs" / f"{test_name}_{pipeline_name}.log",
    )
    original_out, obfuscated_out = (
        RESULTS_DIR / "outputs" / f"{test_name}_original.out",
        RESULTS_DIR / "outputs" / f"{test_name}_{pipeline_name}.out",
    )

    print(f"{Colors.CYAN}  [1/5] Compiling to LLVM IR...{Colors.NC}")
    success, out = run_command(
        [clang, "-O0", "-emit-llvm", "-S", test_c_file, "-o", original_ll], env=env
    )
    if not success:
        print(f"{Colors.RED}  ✗ Failed to compile to IR:\n{out}{Colors.NC}")
        return False

    print(f"{Colors.CYAN}  [2/5] Applying obfuscation passes...{Colors.NC}")
    obfuscation_passes = PASS_PIPELINES[pipeline_name]
    cmd_obfuscate = [
        opt,
        f"-load-pass-plugin={pass_plugin_path}",
        f"-passes={obfuscation_passes}",
        original_ll,
        "-S",
    ]
    try:
        with open(obfuscated_ll, "w") as f_ll, open(log_file, "w") as f_log:
            subprocess.run(
                [str(c) for c in cmd_obfuscate],
                check=True,
                text=True,
                stdout=f_ll,
                stderr=f_log,
                env=env,
            )
    except subprocess.CalledProcessError:
        print(
            f"{Colors.RED}  ✗ Obfuscation pass failed! Check log for details: {
                log_file
            }{Colors.NC}"
        )
        return False

    print(f"{Colors.CYAN}  [3/5] Generating report...{Colors.NC}")
    reporting_passes = f"{obfuscation_passes},chakravyuha-emit-report"
    cmd_report = [
        opt,
        f"-load-pass-plugin={pass_plugin_path}",
        f"-passes={reporting_passes}",
        original_ll,
        "-S",
        "-o",
        os.devnull,
    ]
    try:
        result = subprocess.run(
            [str(c) for c in cmd_report],
            check=True,
            text=True,
            capture_output=True,
            env=env,
        )
        with open(report_json, "w") as f_json:
            f_json.write(result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"{Colors.RED}  ✗ Report generation pass failed!{Colors.NC}\n{e.stderr}")
        return False

    print(f"{Colors.CYAN}  [4/5] Compiling binaries...{Colors.NC}")
    success, out = run_command([clang, original_ll, "-o", original_bin], env=env)
    if not success:
        print(f"{Colors.RED}  ✗ Failed to compile original binary:\n{out}{Colors.NC}")
        return False
    success, out = run_command([clang, obfuscated_ll, "-o", obfuscated_bin], env=env)
    if not success:
        print(f"{Colors.RED}  ✗ Failed to compile obfuscated binary:\n{out}{Colors.NC}")
        return False

    print(f"{Colors.CYAN}  [5/5] Running and comparing output...{Colors.NC}")
    success, out = run_command([original_bin], log_file=original_out, env=env)
    if not success:
        print(f"{Colors.RED}  ✗ Failed to run original binary:\n{out}{Colors.NC}")
        return False
    success, out = run_command([obfuscated_bin], log_file=obfuscated_out, env=env)
    if not success:
        print(f"{Colors.RED}  ✗ Failed to run obfuscated binary:\n{out}{Colors.NC}")
        return False

    with open(original_out, "r") as f1, open(obfuscated_out, "r") as f2:
        if f1.read() == f2.read():
            print(f"{Colors.GREEN}  ✓ Test Passed: Outputs match!{Colors.NC}")
            return True
        else:
            print(f"{Colors.RED}  ✗ Test FAILED: Outputs differ!{Colors.NC}")
            return False


def main():
    parser = argparse.ArgumentParser(
        description="Run the Chakravyuha LLVM Obfuscator test suite."
    )
    parser.add_argument(
        "--pipeline", default="full", choices=list(PASS_PIPELINES.keys())
    )
    args = parser.parse_args()

    if is_wsl():
        print(
            f"{
                Colors.CYAN
            }WSL environment detected. Sanitizing PATH to prioritize Linux toolchain.{
                Colors.NC
            }"
        )
        original_path = os.environ.get("PATH", "")
        # Keep only paths that are Linux-style (e.g., /usr/bin, not /mnt/c/...)
        linux_paths = [
            p
            for p in original_path.split(os.pathsep)
            if p.startswith("/") and not p.startswith("/mnt/")
        ]
        os.environ["PATH"] = os.pathsep.join(linux_paths)

    if platform.system() == "Darwin":
        try:
            result = subprocess.run(
                ["brew", "--prefix", "llvm"], capture_output=True, text=True, check=True
            )
            llvm_bin_dir = Path(result.stdout.strip()) / "bin"
            if llvm_bin_dir.exists():
                print(
                    f"{Colors.CYAN}macOS detected. Prepending Homebrew LLVM to PATH: {
                        llvm_bin_dir
                    }{Colors.NC}"
                )
                os.environ["PATH"] = str(llvm_bin_dir) + os.pathsep + os.environ["PATH"]
        except (subprocess.CalledProcessError, FileNotFoundError):
            print(
                f"{
                    Colors.YELLOW
                }Warning: Could not find Homebrew LLVM. Assuming 'opt' and 'clang' are in the standard PATH.{
                    Colors.NC
                }"
            )

    clang = find_exec(
        "clang",
        "'clang' not found. In WSL, run: sudo apt install clang. On other systems, check your LLVM installation and PATH.",
    )
    opt = find_exec(
        "opt",
        "'opt' from LLVM not found. In WSL, run: sudo apt install llvm. On other systems, check your LLVM installation and PATH.",
    )

    pass_plugin_path = find_pass_plugin()
    env = os.environ.copy()
    llvm_bin_dir = opt.parent
    system = platform.system()
    if system == "Windows":
        print(
            f"{Colors.CYAN}Windows detected. Adding '{
                llvm_bin_dir
            }' to PATH for subprocesses.{Colors.NC}"
        )
        env["PATH"] = str(llvm_bin_dir) + os.pathsep + env.get("PATH", "")
    elif system == "Darwin":
        llvm_lib_dir = llvm_bin_dir.parent / "lib"
        if llvm_lib_dir.exists():
            print(
                f"{Colors.CYAN}macOS detected. Setting DYLD_LIBRARY_PATH to '{
                    llvm_lib_dir
                }'.{Colors.NC}"
            )
            env["DYLD_LIBRARY_PATH"] = (
                str(llvm_lib_dir) + os.pathsep + env.get("DYLD_LIBRARY_PATH", "")
            )

    for subdir in ["ll_files", "binaries", "reports", "logs", "outputs"]:
        (RESULTS_DIR / subdir).mkdir(parents=True, exist_ok=True)

    test_files = sorted(list(TEST_SRC_DIR.glob("test_*.c")))
    passed_count, failed_count = 0, 0
    for test_file in test_files:
        if run_test(test_file, args.pipeline, pass_plugin_path, clang, opt, env):
            passed_count += 1
        else:
            failed_count += 1

    print(
        "\n"
        + "=" * 50
        + f"\n        Test Summary (Pipeline: {args.pipeline})\n"
        + "=" * 50
    )
    print(f"{Colors.GREEN}Passed: {passed_count}{Colors.NC}")
    print(f"{Colors.RED}Failed: {failed_count}{Colors.NC}")
    print("=" * 50)
    if failed_count > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
