#!/usr/bin/env python3
import json
import os
import platform
import shutil
import subprocess
import sys
import webbrowser
from argparse import ArgumentParser, RawDescriptionHelpFormatter
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

# --- optional dependency: tqdm for progress bars ---
try:
    from tqdm import tqdm
except ImportError:
    print(
        "info: 'tqdm' library not found. progress bar will not be shown.",
        file=sys.stderr,
    )
    print("      to install it, run: pip3 install tqdm", file=sys.stderr)

    def tqdm(iterable, **_kwargs):
        return iterable


# --- configuration ---
script_dir = Path(__file__).parent.resolve()
project_root = script_dir
build_dir = project_root / "build"
results_dir = script_dir / "test_results"


# --- helper functions ---
def find_executable(name, msg):
    if not shutil.which(name):
        print(f"error: {msg}", file=sys.stderr)
        sys.exit(1)
    return name


def format_bytes(size):
    if size is None or size < 0:
        return "n/a"
    if size == 0:
        return "0 B"
    power = 1024
    n = 0
    labels = {0: "B", 1: "KB", 2: "MB", 3: "GB", 4: "TB"}
    while size >= power and n < len(labels) - 1:
        size /= power
        n += 1
    return f"{size:.2f} {labels[n]}"


def sanitize_func_name(name):
    """Replaces characters invalid in filenames with underscores."""
    return name.replace(":", "_").replace("<", "_").replace(">", "_").replace(" ", "_")


def desanitize_func_name(name):
    return name.replace("__", "::")


# --- parallel rendering task ---
def render_dot_to_png(dot_file_info):
    dot_file, dot_executable = dot_file_info
    is_original = dot_file.parent.name == "original"
    viz_dir = (
        results_dir / "visualizations" / ("original" if is_original else "obfuscated")
    )
    png_file = viz_dir / f"{dot_file.stem}.png"

    subprocess.run(
        [dot_executable, "-Tpng", str(dot_file), "-o", str(png_file)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return True


# --- core logic ---
def generate_visualizations():
    print("checking for required tools...")
    opt = find_executable(
        "opt", "'opt' (from llvm) not found. check your llvm installation and path."
    )
    dot = find_executable(
        "dot",
        "'dot' (from graphviz) not found. is graphviz installed and in your path?",
    )

    ll_dir = results_dir / "ll_files"
    if not ll_dir.exists() or not any(ll_dir.glob("*.ll")):
        print(
            f"error: llvm ir directory '{ll_dir}' is empty or not found.",
            file=sys.stderr,
        )
        print(
            "please run the test suite first to generate the necessary ir files.",
            file=sys.stderr,
        )
        return False

    orig_dot_dir = results_dir / "dot_files" / "original"
    obf_dot_dir = results_dir / "dot_files" / "obfuscated"
    orig_viz_dir = results_dir / "visualizations" / "original"
    obf_viz_dir = results_dir / "visualizations" / "obfuscated"
    for d in [orig_dot_dir, obf_dot_dir, orig_viz_dir, obf_viz_dir]:
        d.mkdir(parents=True, exist_ok=True)

    print("generating cfg .dot files...")
    for old_dot in script_dir.glob("*.dot"):
        old_dot.unlink()

    for ll_file in ll_dir.glob("*.ll"):
        subprocess.run(
            [opt, "-passes=dot-cfg", str(ll_file)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            cwd=script_dir,
        )

        test_name = ll_file.stem
        is_obfuscated = any(
            f"_{suf}" in test_name for suf in ["cff", "string", "fake", "full"]
        )
        target_dir = obf_dot_dir if is_obfuscated else orig_dot_dir
        base_name = "_".join(test_name.split("_")[:-1]) if is_obfuscated else test_name

        for dot_file in script_dir.glob("*.dot"):
            func_name = dot_file.stem.strip(".")
            sanitized_name = sanitize_func_name(func_name)
            new_name = f"{base_name}_{sanitized_name}.dot"
            dot_file.rename(target_dir / new_name)

    print("rendering cfg images in parallel...")
    all_dots = list(orig_dot_dir.glob("*.dot")) + list(obf_dot_dir.glob("*.dot"))
    if not all_dots:
        print("Warning: No .dot files found to render.")
        return True

    with ProcessPoolExecutor() as executor:
        tasks = [(dot_file, dot) for dot_file in all_dots]
        list(
            tqdm(
                executor.map(render_dot_to_png, tasks),
                total=len(tasks),
                desc="rendering images",
            )
        )

    print("image rendering complete.")
    return True


def create_comparison_html():
    print("generating interactive html report...")
    comparison_dir = results_dir / "visualizations" / "comparison"
    comparison_dir.mkdir(parents=True, exist_ok=True)

    tests = {}
    orig_dir = results_dir / "visualizations" / "original"
    obf_dir = results_dir / "visualizations" / "obfuscated"

    c_test_files = (project_root / "tests").glob("test_*.c")
    cpp_test_files = (project_root / "tests").glob("test_*.cpp")
    all_test_files = list(c_test_files) + list(cpp_test_files)
    known_test_names = sorted([p.stem for p in all_test_files])

    if orig_dir.exists():
        for orig_img in orig_dir.glob("*.png"):
            for test_name in known_test_names:
                prefix = f"{test_name}_"
                if orig_img.stem.startswith(prefix):
                    sanitized_func_name = orig_img.stem[len(prefix) :]
                    obf_img = obf_dir / orig_img.name
                    if obf_img.exists():
                        if test_name not in tests:
                            tests[test_name] = {}

                        # Store by sanitized name for lookup, but display desanitized name
                        tests[test_name][sanitized_func_name] = {
                            "display_name": desanitize_func_name(sanitized_func_name),
                            "original": str(orig_img.relative_to(results_dir)),
                            "obfuscated": str(obf_img.relative_to(results_dir)),
                        }
                        break

    metrics = {}
    reports_dir = results_dir / "reports"
    binaries_dir = results_dir / "binaries"

    if reports_dir.exists():
        for test_name in known_test_names:
            report_to_load = None
            report_type = "full"  # Prioritize 'full' report for consistency

            potential_path = reports_dir / f"{test_name}_{report_type}.json"
            if potential_path.exists():
                report_to_load = potential_path

            if report_to_load:
                try:
                    with open(report_to_load, "r") as f:
                        metrics[test_name] = json.load(f)

                    exe_suffix = ".exe" if platform.system() == "Windows" else ""
                    orig_bin = binaries_dir / f"{test_name}_original{exe_suffix}"
                    obf_bin = binaries_dir / f"{test_name}_{report_type}{exe_suffix}"

                    orig_size = orig_bin.stat().st_size if orig_bin.exists() else None
                    obf_size = obf_bin.stat().st_size if obf_bin.exists() else None

                    change_str = "N/A"
                    if orig_size is not None and obf_size is not None and orig_size > 0:
                        change_pct = (obf_size - orig_size) / orig_size * 100
                        change_str = f"{change_pct:+.2f}%"

                    metrics[test_name]["binary_metrics"] = {
                        "original_size": format_bytes(orig_size),
                        "obfuscated_size": format_bytes(obf_size),
                        "change_pct": change_str,
                    }
                except (json.JSONDecodeError, FileNotFoundError):
                    metrics[test_name] = {}
            else:
                metrics[test_name] = {}

    test_options = "\n".join(
        f'                <option value="{name}">{name}</option>'
        for name in sorted(tests.keys())
    )

    html_content = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Chakravyuha - Visual Comparison Report</title>
    <style>
        :root {{
            --bg-color: #f8f9fa;
            --container-bg: #ffffff;
            --header-bg: #2c3e50;
            --text-color: #343a40;
            --primary-color: #3498db;
            --primary-hover: #2980b9;
            --border-color: #dee2e6;
            --error-color: #e74c3c;
        }}
        * {{ margin: 0; padding: 0; box-sizing: border-box; }}
        body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif; background-color: var(--bg-color); color: var(--text-color); padding: 20px; min-height: 100vh; }}
        .container {{ max-width: 1600px; margin: 0 auto; background: var(--container-bg); border-radius: 8px; box-shadow: 0 6px 20px rgba(0, 0, 0, 0.08); overflow: hidden; }}

        .header {{ background-color: var(--header-bg); color: white; padding: 2rem; text-align: center; }}
        .header .logo {{ width: 150px; height: 150px; margin: 0 auto 1rem; border-radius: 8px; display: block; }}
        .header .tagline {{ font-style: italic; font-size: 1.2rem; margin-top: 0; margin-bottom: 0.25rem; color: #ecf0f1; }}
        .header p {{ margin: 0; color: #bdc3c7; }}
        h1 {{ font-size: 2em; margin-top: 1rem; font-weight: 600; }}

        .controls {{ padding: 20px; background: #fdfdff; border-bottom: 1px solid var(--border-color); display: flex; gap: 20px; align-items: center; flex-wrap: wrap; }}
        .controls select, .controls button {{ padding: 10px 18px; border-radius: 6px; border: 1px solid var(--border-color); background: white; font-size: 16px; font-weight: 500; cursor: pointer; transition: all 0.2s ease; }}
        .controls select {{ min-width: 250px; }}
        .controls select:hover, .controls select:focus {{ border-color: var(--primary-color); outline: none; box-shadow: 0 0 0 3px rgba(52, 152, 219, 0.1); }}
        .controls button {{ background-color: var(--primary-color); color: white; border-color: var(--primary-color); }}
        .controls button:hover {{ background-color: var(--primary-hover); border-color: var(--primary-hover); }}

        .metrics {{ padding: 25px; background-color: #fdfdff; display: none; border-bottom: 1px solid var(--border-color); }}
        .metrics.show {{ display: block; }}
        .metrics-grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; }}
        .metric-card {{ background: var(--bg-color); padding: 20px; border-radius: 8px; text-align: center; border: 1px solid var(--border-color); }}
        .metric-card .value {{ font-size: 2em; font-weight: 600; color: var(--primary-color); margin-bottom: 8px; }}
        .metric-card .value.small-text {{ font-size: 1.1em; line-height: 1.4; font-weight: 500; color: var(--text-color); }}
        .metric-card .label {{ color: #495057; font-size: 0.9em; font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px; }}
        .metric-card .sub-label {{ color: #6c757d; font-size: 0.8em; margin-top: 4px; }}

        .comparison-container {{ padding: 30px; }}
        .image-container {{ display: flex; gap: 30px; justify-content: center; align-items: flex-start; min-height: 400px; }}
        .image-wrapper {{ flex: 1; text-align: center; max-width: 50%; }}
        .image-wrapper h3 {{ margin-bottom: 15px; color: var(--header-bg); font-size: 1.4em; font-weight: 600; padding-bottom: 10px; border-bottom: 2px solid var(--border-color); }}
        .image-wrapper img {{ max-width: 100%; height: auto; border: 1px solid var(--border-color); border-radius: 8px; background: white; box-shadow: 0 4px 15px rgba(0,0,0,0.07); }}

        .no-data {{ text-align: center; padding: 80px 40px; color: #6c757d; font-size: 1.2em; }}
        .footer {{ padding: 20px; text-align: center; background-color: var(--header-bg); color: #bdc3c7; font-size: 0.9em; border-top: 1px solid var(--border-color);}}
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <img src="../../../assets/chakravyuha_logo.png" alt="Chakravyuha Logo" class="logo">
            <p class="tagline">The Impenetrable Code Formation</p>
            <h1>Visual Comparison Report</h1>
        </div>
        <div class="controls">
            <select id="testSelect"><option value="">Select a test...</option>{test_options}</select>
            <select id="functionSelect" disabled><option value="">Select a function...</option></select>
            <button onclick="toggleMetrics()">📊 Show Full Report</button>
        </div>
        <div class="metrics" id="metrics"><div class="metrics-grid" id="metricsGrid"></div></div>
        <div class="comparison-container"><div id="imageContainer" class="image-container"><div class="no-data">Select a test and function to view comparison</div></div></div>
        <div class="footer"><p>Generated by the Chakravyuha LLVM Obfuscator</p></div>
    </div>
    <script>
        const tests = {json.dumps(tests, indent=4)};
        const metrics = {json.dumps(metrics, indent=4)};
        let currentTest = '';
        let currentFunction = '';
        const testSelect = document.getElementById('testSelect');
        const funcSelect = document.getElementById('functionSelect');
        const imageContainer = document.getElementById('imageContainer');
        const metricsDiv = document.getElementById('metrics');
        const metricsGrid = document.getElementById('metricsGrid');

        testSelect.addEventListener('change', function(e) {{
            currentTest = e.target.value;
            updateFunctionList();
            updateDisplay();
            if (metricsDiv.classList.contains('show')) {{ updateMetrics(); }}
        }});

        funcSelect.addEventListener('change', function(e) {{
            currentFunction = e.target.value;
            updateDisplay();
        }});

        function updateFunctionList() {{
            funcSelect.innerHTML = '<option value="">Select a function...</option>';
            funcSelect.disabled = true;
            if (currentTest && tests[currentTest]) {{
                funcSelect.disabled = false;
                const functions = Object.keys(tests[currentTest]).sort((a, b) => a.localeCompare(b, undefined, {{numeric: true}}));
                functions.forEach(funcKey => {{
                    const option = document.createElement('option');
                    option.value = funcKey;
                    option.textContent = tests[currentTest][funcKey].display_name;
                    funcSelect.appendChild(option);
                }});
                if (functions.length > 0) {{
                    // Auto-select the first function (often 'main')
                    const mainFunc = functions.find(f => tests[currentTest][f].display_name === 'main') || functions[0];
                    funcSelect.value = mainFunc;
                    currentFunction = mainFunc;
                }}
            }}
        }}

        function updateDisplay() {{
            if (!currentTest || !currentFunction || !tests[currentTest]?.[currentFunction]) {{
                imageContainer.innerHTML = '<div class="no-data">Select a test and function to view comparison</div>';
            }} else {{
                const images = tests[currentTest][currentFunction];
                imageContainer.innerHTML = `
                    <div class="image-wrapper"><h3>Original</h3><img src="../../${{images.original}}" alt="Original CFG"></div>
                    <div class="image-wrapper"><h3>Obfuscated</h3><img src="../../${{images.obfuscated}}" alt="Obfuscated CFG"></div>`;
            }}
        }}

        function toggleMetrics() {{
            if (!metricsDiv.classList.contains('show')) {{ updateMetrics(); }}
            metricsDiv.classList.toggle('show');
        }}

        function updateMetrics() {{
            if (currentTest && metrics[currentTest] && Object.keys(metrics[currentTest]).length > 0) {{
                const m = metrics[currentTest];
                const o = m.obfuscationMetrics || {{}};
                const cff = o.controlFlowFlattening || {{ "flattenedFunctions": 0, "flattenedBlocks": 0 }};
                const se = o.stringEncryption || {{ "count": 0 }};
                const fci = o.fakeCodeInsertion || {{ "insertedBlocks": 0 }};
                const passes = (o.passesRun || []).join('<br>') || 'None';
                const attrs = m.outputAttributes || {{}};
                const bin = m.binary_metrics || {{}};
                metricsGrid.innerHTML = `
                    <div class="metric-card"><div class="value">${{bin.change_pct || 'N/A'}}</div><div class="label">Exec Size Change</div><div class="sub-label">${{bin.original_size || '?'}} &rarr; ${{bin.obfuscated_size || '?'}}</div></div>
                    <div class="metric-card"><div class="value">${{attrs.totalIRSizeChange || 'N/A'}}</div><div class="label">IR Size Change</div><div class="sub-label">${{attrs.originalIRSize || '?'}} &rarr; ${{attrs.obfuscatedIRSize || '?'}}</div></div>
                    <div class="metric-card"><div class="value">${{cff.flattenedFunctions}}</div><div class="label">Flattened Funcs</div></div>
                    <div class="metric-card"><div class="value">${{cff.flattenedBlocks}}</div><div class="label">Flattened Blocks</div></div>
                    <div class="metric-card"><div class="value">${{se.count}}</div><div class="label">Strings Encrypted</div></div>
                    <div class="metric-card"><div class="value">${{fci.insertedBlocks}}</div><div class="label">Fake Blocks</div></div>
                    <div class="metric-card"><div class="value small-text">${{passes}}</div><div class="label">Passes Run</div></div>
                    <div class="metric-card"><div class="value">${{o.cyclesCompleted || 1}}</div><div class="label">Cycles</div></div>`;
            }} else {{
                metricsGrid.innerHTML = `<div class="metric-card" style="grid-column: 1 / -1;"><div class="label">No report data. Select a test to see its metrics.</div></div>`;
            }}
        }}

        // Initial setup
        (function() {{
            if (testSelect.options.length > 1) {{
                testSelect.value = testSelect.options[1].value;
                currentTest = testSelect.value;
                updateFunctionList();
                updateDisplay();
            }}
            updateMetrics();
        }})();
    </script>
</body>
</html>"""

    html_file = comparison_dir / "index.html"
    with open(html_file, "w", encoding="utf-8") as f:
        f.write(html_content)
    print(f"created comparison viewer at {html_file}")
    return html_file


def view_report(report_path):
    uri = report_path.resolve().as_uri()
    print(f"opening report in default browser: {uri}")
    webbrowser.open(uri)


def main():
    if platform.system() == "Darwin":
        try:
            result = subprocess.run(
                ["brew", "--prefix", "llvm"], capture_output=True, text=True, check=True
            )
            llvm_bin_dir = Path(result.stdout.strip()) / "bin"
            if llvm_bin_dir.exists():
                os.environ["PATH"] = str(llvm_bin_dir) + os.pathsep + os.environ["PATH"]
        except (subprocess.CalledProcessError, FileNotFoundError):
            print("warning: could not find homebrew llvm. assuming tools are in path.")

    parser = ArgumentParser(
        description="chakravyuha obfuscator visualization pipeline",
        formatter_class=RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "action",
        nargs="?",
        default="visualize",
        choices=["visualize", "view"],
        help="action to perform (default: visualize).",
    )
    args = parser.parse_args()

    if generate_visualizations():
        report_path = create_comparison_html()
        if args.action == "view":
            view_report(report_path)


if __name__ == "__main__":
    main()
