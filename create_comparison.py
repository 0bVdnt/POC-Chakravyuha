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

# --- Optional Dependency: tqdm for progress bars ---
try:
    from tqdm import tqdm
except ImportError:
    print(
        "Info: 'tqdm' library not found. Progress bar will not be shown.",
        file=sys.stderr,
    )
    print("      To install it, run: pip3 install tqdm", file=sys.stderr)

    def tqdm(iterable, **kwargs):
        return iterable


# --- Configuration ---
SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR
BUILD_DIR = PROJECT_ROOT / "build"
RESULTS_DIR = SCRIPT_DIR / "test_results"


# --- Helper Functions ---
def find_executable(name, msg):
    if not shutil.which(name):
        print(f"Error: {msg}", file=sys.stderr)
        sys.exit(1)
    return name


def format_bytes(size):
    if size is None or size < 0:
        return "N/A"
    if size == 0:
        return "0 B"
    power = 1024
    n = 0
    labels = {0: "B", 1: "KB", 2: "MB", 3: "GB", 4: "TB"}
    while size >= power and n < len(labels) - 1:
        size /= power
        n += 1
    return f"{int(size)} {labels[n]}" if n == 0 else f"{size:.2f} {labels[n]}"


# --- Parallel Rendering Task ---
def render_dot_to_png(dot_file, dot_executable):
    is_original = dot_file.parent.name == "original"
    viz_dir = (
        RESULTS_DIR / "visualizations" / ("original" if is_original else "obfuscated")
    )
    png_file = viz_dir / f"{dot_file.stem}.png"
    subprocess.run(
        [dot_executable, "-Tpng", str(dot_file), "-o", str(png_file)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return True


# --- Core Logic ---
def generate_visualizations():
    print("Checking for required tools...")
    opt = find_executable(
        "opt", "'opt' (from LLVM) not found. Check your LLVM installation and PATH."
    )
    dot = find_executable(
        "dot",
        "'dot' (from Graphviz) not found. Is Graphviz installed and in your PATH?",
    )

    ll_dir = RESULTS_DIR / "ll_files"
    if not ll_dir.exists() or not any(ll_dir.glob("*.ll")):
        print(
            f"Error: LLVM IR directory '{ll_dir}' is empty or not found.",
            file=sys.stderr,
        )
        print(
            "Please run the test suite first to generate the necessary IR files.",
            file=sys.stderr,
        )
        return False

    orig_dot_dir, obf_dot_dir = (
        RESULTS_DIR / "dot_files" / "original",
        RESULTS_DIR / "dot_files" / "obfuscated",
    )
    orig_viz_dir, obf_viz_dir = (
        RESULTS_DIR / "visualizations" / "original",
        RESULTS_DIR / "visualizations" / "obfuscated",
    )
    for d in [orig_dot_dir, obf_dot_dir, orig_viz_dir, obf_viz_dir]:
        d.mkdir(parents=True, exist_ok=True)

    print("Generating CFG .dot files...")
    for old_dot in SCRIPT_DIR.glob("*.dot"):
        old_dot.unlink()

    for ll_file in ll_dir.glob("*.ll"):
        subprocess.run(
            [opt, "-passes=dot-cfg", str(ll_file), "-o", os.devnull],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            cwd=SCRIPT_DIR,
        )

        test_name = ll_file.stem
        is_obfuscated = any(
            f"_{suf}" in test_name for suf in ["cff", "string", "fake", "full"]
        )

        for dot_file in SCRIPT_DIR.glob("*.dot"):
            func_name = dot_file.stem.strip(".")
            base_name = (
                "_".join(test_name.split("_")[:-1]) if is_obfuscated else test_name
            )
            new_name = f"{base_name}_{func_name}.dot"
            target_dir = obf_dot_dir if is_obfuscated else orig_dot_dir
            dot_file.rename(target_dir / new_name)

    print("Rendering CFG images in parallel...")
    all_dots = [
        f
        for d in [orig_dot_dir, obf_dot_dir]
        for f in d.glob("*.dot")
        if not any(n in f.stem for n in ["chakravyuha_", "dispatch_"])
    ]

    with ProcessPoolExecutor() as executor:
        list(
            tqdm(
                executor.map(render_dot_to_png, all_dots, [dot] * len(all_dots)),
                total=len(all_dots),
                desc="Rendering images",
            )
        )

    print("Image rendering complete.")
    return True


def create_comparison_html():
    print("Generating interactive HTML report...")
    results_dir = Path("test_results")
    viz_dir = Path("test_results/visualizations")
    comparison_dir = viz_dir / "comparison"
    comparison_dir.mkdir(parents=True, exist_ok=True)

    tests = {}
    orig_dir = viz_dir / "original"
    obf_dir = viz_dir / "obfuscated"
    known_test_names = sorted(
        [p.stem for p in (PROJECT_ROOT / "tests").glob("test_*.c")]
    )

    if orig_dir.exists():
        for orig_img in orig_dir.glob("*.png"):
            for test_name in known_test_names:
                prefix = f"{test_name}_"
                if orig_img.stem.startswith(prefix):
                    func_name = orig_img.stem[len(prefix) :]
                    obf_img = obf_dir / orig_img.name
                    if obf_img.exists():
                        if test_name not in tests:
                            tests[test_name] = {}
                        tests[test_name][func_name] = {
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
            report_type = None

            for r_type in ["full", "cff", "string", "fake"]:
                potential_path = reports_dir / f"{test_name}_{r_type}.json"
                if potential_path.exists():
                    report_to_load = potential_path
                    report_type = r_type
                    break

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
                    if (
                        all(s is not None for s in [orig_size, obf_size])
                        and orig_size > 0
                    ):
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

    # Generate test options HTML
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
        * {{
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }}

        body {{
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: #f4f7f6;
            color: #333;
            padding: 20px;
        }}

        .container {{
            max-width: 1600px;
            margin: 0 auto;
            background: white;
            border-radius: 20px;
            box-shadow: 0 10px 40px rgba(0, 0, 0, 0.1);
            overflow: hidden;
        }}

        .header {{
            background: linear-gradient(135deg, #5A78E1 0%, #324172 100%);
            color: white;
            padding: 40px;
            text-align: center;
        }}

        .header h1 {{
            font-size: 2.8em;
            margin-bottom: 10px;
        }}

        .header p {{
            font-size: 1.2em;
            opacity: 0.9;
        }}

        .controls {{
            padding: 20px;
            background: #f8f9fa;
            border-bottom: 1px solid #dee2e6;
            display: flex;
            gap: 20px;
            align-items: center;
            flex-wrap: wrap;
        }}

        .controls select,
        .controls button {{
            padding: 12px 20px;
            border-radius: 8px;
            border: 1px solid #dee2e6;
            background: white;
            font-size: 16px;
            cursor: pointer;
        }}

        .controls button {{
            background: #5A78E1;
            color: white;
            border: none;
            transition: background 0.3s;
        }}

        .controls button:hover {{
            background: #324172;
        }}

        .metrics {{
            padding: 30px;
            background: #e9ecef;
            display: none;
        }}

        .metrics.show {{
            display: block;
        }}

        .metrics-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
        }}

        .metric-card {{
            background: white;
            padding: 20px;
            border-radius: 12px;
            text-align: center;
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.05);
        }}

        .metric-card .value {{
            font-size: 2.2em;
            font-weight: bold;
            color: #5A78E1;
        }}

        .metric-card .label {{
            color: #6c757d;
            margin-top: 8px;
            font-size: 0.9em;
        }}

        .metric-card .sub-label {{
            color: #999;
            font-size: 0.8em;
            margin-top: 4px;
        }}

        .comparison-container {{
            padding: 30px;
        }}

        .image-container {{
            display: flex;
            gap: 20px;
            justify-content: center;
            align-items: flex-start;
            min-height: 400px;
        }}

        .image-wrapper {{
            flex: 1;
            text-align: center;
        }}

        .image-wrapper h3 {{
            margin-bottom: 15px;
            color: #495057;
            font-size: 1.4em;
        }}

        .image-wrapper img {{
            max-width: 100%;
            height: auto;
            border: 2px solid #dee2e6;
            border-radius: 10px;
            background: white;
        }}

        .no-data {{
            text-align: center;
            padding: 50px;
            color: #6c757d;
            font-size: 1.2em;
        }}

        .footer {{
            padding: 20px;
            text-align: center;
            background: #f8f9fa;
            color: #6c757d;
        }}
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🔒 Chakravyuha Obfuscation Report</h1>
            <p>Visual comparison of original vs. obfuscated control flow graphs</p>
        </div>

        <div class="controls">
            <select id="testSelect">
                <option value="">Select a test...</option>
{test_options}
            </select>
            <select id="functionSelect" disabled>
                <option value="">Select a function...</option>
            </select>
            <button onclick="toggleMetrics()">📊 Show Full Report</button>
        </div>

        <div class="metrics" id="metrics">
            <div class="metrics-grid" id="metricsGrid"></div>
        </div>

        <div class="comparison-container">
            <div id="imageContainer" class="image-container">
                <div class="no-data">Select a test and function to view comparison</div>
            </div>
        </div>

        <div class="footer">
            <p>Generated by the Chakravyuha LLVM Obfuscator</p>
        </div>
    </div>

    <script>
        const tests = {json.dumps(tests, indent=8)};
        const metrics = {json.dumps(metrics, indent=8)};

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
                const functions = Object.keys(tests[currentTest]).sort((a, b) => {{
                    return a.localeCompare(b, undefined, {{numeric: true}});
                }});

                functions.forEach(func => {{
                    const option = document.createElement('option');
                    option.value = func;
                    option.textContent = func.replace(/_/g, ' ');
                    funcSelect.appendChild(option);
                }});
            }}
        }}

        function updateDisplay() {{
            if (!currentTest || !currentFunction || !tests[currentTest]?.[currentFunction]) {{
                imageContainer.innerHTML = '<div class="no-data">Select a test and function to view comparison</div>';
            }} else {{
                const images = tests[currentTest][currentFunction];
                imageContainer.innerHTML = `
                    <div class="image-wrapper">
                        <h3>Original</h3>
                        <img src="../../${{images.original}}" alt="Original CFG">
                    </div>
                    <div class="image-wrapper">
                        <h3>Obfuscated</h3>
                        <img src="../../${{images.obfuscated}}" alt="Obfuscated CFG">
                    </div>
                `;
            }}

            if (metricsDiv.classList.contains('show')) {{
                updateMetrics();
            }}
        }}

        function toggleMetrics() {{
            if (!metricsDiv.classList.contains('show')) {{
                updateMetrics();
            }}
            metricsDiv.classList.toggle('show');
        }}

        function updateMetrics() {{
            if (currentTest && metrics[currentTest] && Object.keys(metrics[currentTest]).length > 0) {{
                const m = metrics[currentTest];
                const o = m.obfuscationMetrics || {{}};
                const cff = o.controlFlowFlattening || {{}};
                const se = o.stringEncryption || {{}};
                const fci = o.fakeCodeInsertion || {{}};
                const passes = (o.passesRun || []).join('<br>') || 'None';
                const attrs = m.outputAttributes || {{}};
                const bin = m.binary_metrics || {{}};

                metricsGrid.innerHTML = `
                    <div class="metric-card">
                        <div class="value">${{cff.flattenedFunctions || 0}}</div>
                        <div class="label">Flattened Functions</div>
                    </div>
                    <div class="metric-card">
                        <div class="value">${{cff.flattenedBlocks || 0}}</div>
                        <div class="label">Flattened Blocks</div>
                    </div>
                    <div class="metric-card">
                        <div class="value">${{se.count || 0}}</div>
                        <div class="label">Strings Encrypted</div>
                        <div class="sub-label">${{se.method || 'N/A'}}</div>
                    </div>
                    <div class="metric-card">
                        <div class="value">${{fci.insertedBlocks || 0}}</div>
                        <div class="label">Fake Blocks Inserted</div>
                    </div>
                    <div class="metric-card">
                        <div class="value">${{attrs.totalIRSizeChange || 'N/A'}}</div>
                        <div class="label">Total IR Size Change</div>
                        <div class="sub-label">${{attrs.originalIRSize || '?'}} → ${{attrs.obfuscatedIRSize || '?'}}</div>
                    </div>
                    <div class="metric-card">
                        <div class="value">${{bin.change_pct || 'N/A'}}</div>
                        <div class="label">Executable Size Change</div>
                        <div class="sub-label">${{bin.original_size || '?'}} → ${{bin.obfuscated_size || '?'}}</div>
                    </div>
                    <div class="metric-card" style="grid-column: span 2;">
                        <div class="value" style="font-size: 1.1em; line-height: 1.5;">${{passes}}</div>
                        <div class="label">Passes Run</div>
                    </div>
                    <div class="metric-card" style="grid-column: 1 / -1; background: #f8f9fa;">
                        <div class="value" style="font-size: 1.5em;">${{attrs.stringDataSizeChange || '0.00%'}}</div>
                        <div class="label">IR String Data Size Change</div>
                        <div class="sub-label">Note: 0% is expected as ciphers preserve length</div>
                        <div class="sub-label">${{attrs.originalIRStringDataSize || '0'}} → ${{attrs.obfuscatedIRStringDataSize || '0'}}</div>
                    </div>
                `;
            }} else {{
                metricsGrid.innerHTML = `
                    <div class="metric-card" style="grid-column: 1 / -1;">
                        <div class="label">No report data available for this test.</div>
                    </div>
                `;
            }}
        }}
    </script>
</body>
</html>"""

    html_file = comparison_dir / "index.html"
    with open(html_file, "w", encoding="utf-8") as f:
        f.write(html_content)
    print(f"Created comparison viewer at {html_file}")
    return html_file


def view_report(report_path):
    uri = report_path.resolve().as_uri()
    print(f"Opening report in default browser: {uri}")
    webbrowser.open(uri)


def main():
    if platform.system() == "Darwin":
        try:
            result = subprocess.run(
                ["brew", "--prefix", "llvm"], capture_output=True, text=True, check=True
            )
            llvm_bin_dir = Path(result.stdout.strip()) / "bin"
            if llvm_bin_dir.exists():
                print(
                    f"macOS detected. Prepending Homebrew LLVM to PATH: {llvm_bin_dir}"
                )
                os.environ["PATH"] = str(llvm_bin_dir) + os.pathsep + os.environ["PATH"]
        except (subprocess.CalledProcessError, FileNotFoundError):
            print(
                "Warning: Could not find Homebrew LLVM. "
                "Assuming 'opt' and 'dot' are in the standard PATH."
            )

    parser = ArgumentParser(
        description="Chakravyuha Obfuscator Visualization Pipeline",
        formatter_class=RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "action",
        nargs="?",
        default="visualize",
        choices=["visualize", "view"],
        help="Action to perform (default: visualize). 'view' opens the report in a browser.",
    )
    args = parser.parse_args()

    if generate_visualizations():
        report_path = create_comparison_html()
        if args.action == "view":
            view_report(report_path)


if __name__ == "__main__":
    main()
