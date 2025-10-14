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
        return "0 b"
    power = 1024
    n = 0
    labels = {0: "b", 1: "kb", 2: "mb", 3: "gb", 4: "tb"}
    while size >= power and n < len(labels) - 1:
        size /= power
        n += 1
    return f"{int(size)} {labels[n]}" if n == 0 else f"{size:.2f} {labels[n]}"


# --- parallel rendering task ---
def render_dot_to_png(dot_file, dot_executable):
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

    orig_dot_dir, obf_dot_dir = (
        results_dir / "dot_files" / "original",
        results_dir / "dot_files" / "obfuscated",
    )
    orig_viz_dir, obf_viz_dir = (
        results_dir / "visualizations" / "original",
        results_dir / "visualizations" / "obfuscated",
    )
    for d in [orig_dot_dir, obf_dot_dir, orig_viz_dir, obf_viz_dir]:
        d.mkdir(parents=True, exist_ok=True)

    print("generating cfg .dot files...")
    for old_dot in script_dir.glob("*.dot"):
        old_dot.unlink()

    for ll_file in ll_dir.glob("*.ll"):
        subprocess.run(
            [opt, "-passes=dot-cfg", str(ll_file), "-o", os.devnull],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            cwd=script_dir,
        )

        test_name = ll_file.stem
        is_obfuscated = any(
            f"_{suf}" in test_name for suf in ["cff", "string", "fake", "full"]
        )

        for dot_file in script_dir.glob("*.dot"):
            func_name = dot_file.stem.strip(".")
            base_name = (
                "_".join(test_name.split("_")[:-1]) if is_obfuscated else test_name
            )
            new_name = f"{base_name}_{func_name}.dot"
            target_dir = obf_dot_dir if is_obfuscated else orig_dot_dir
            dot_file.rename(target_dir / new_name)

    print("rendering cfg images in parallel...")
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
                desc="rendering images",
            )
        )

    print("image rendering complete.")
    return True


def create_comparison_html():
    print("generating interactive html report...")
    results_dir = Path("test_results")
    viz_dir = Path("test_results/visualizations")
    comparison_dir = viz_dir / "comparison"
    comparison_dir.mkdir(parents=True, exist_ok=True)

    tests = {}
    orig_dir = viz_dir / "original"
    obf_dir = viz_dir / "obfuscated"
    known_test_names = sorted(
        [p.stem for p in (project_root / "tests").glob("test_*.c")]
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

                    change_str = "n/a"
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

    # generate test options html
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
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
            background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
            color: #2c3e50;
            padding: 20px;
            min-height: 100vh;
        }}

        .container {{
            max-width: 1600px;
            margin: 0 auto;
            background: white;
            border-radius: 16px;
            box-shadow: 0 20px 60px rgba(0, 0, 0, 0.15);
            overflow: hidden;
        }}

        .header {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 50px 40px;
            text-align: center;
        }}

        .header h1 {{
            font-size: 3em;
            margin-bottom: 12px;
            font-weight: 700;
            text-shadow: 0 2px 10px rgba(0,0,0,0.2);
        }}

        .header p {{
            font-size: 1.3em;
            opacity: 0.95;
        }}

        .controls {{
            padding: 30px;
            background: #f8f9fa;
            border-bottom: 2px solid #e9ecef;
            display: flex;
            gap: 20px;
            align-items: center;
            flex-wrap: wrap;
        }}

        .controls select,
        .controls button {{
            padding: 14px 24px;
            border-radius: 10px;
            border: 2px solid #dee2e6;
            background: white;
            font-size: 16px;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.3s ease;
            box-shadow: 0 2px 5px rgba(0,0,0,0.05);
        }}

        .controls select {{
            min-width: 250px;
        }}

        .controls select:hover,
        .controls select:focus {{
            border-color: #667eea;
            outline: none;
            box-shadow: 0 4px 12px rgba(102, 126, 234, 0.2);
        }}

        .controls button {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            font-weight: 600;
        }}

        .controls button:hover {{
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(102, 126, 234, 0.4);
        }}

        .metrics {{
            padding: 40px;
            background: linear-gradient(135deg, #f5f7fa 0%, #e9ecef 100%);
            display: none;
            border-bottom: 2px solid #dee2e6;
        }}

        .metrics.show {{
            display: block;
        }}

        .metrics-grid {{
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 25px;
        }}

        .metric-card {{
            background: white;
            padding: 25px;
            border-radius: 14px;
            text-align: center;
            box-shadow: 0 6px 20px rgba(0, 0, 0, 0.08);
            transition: all 0.3s ease;
            border: 1px solid #e9ecef;
        }}

        .metric-card:hover {{
            transform: translateY(-5px);
            box-shadow: 0 12px 30px rgba(0, 0, 0, 0.15);
        }}

        .metric-card .value {{
            font-size: 2.2em;
            font-weight: 700;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
            margin-bottom: 10px;
        }}

        .metric-card .value.small-text {{
             font-size: 1.1em;
             line-height: 1.5;
             font-weight: 500;
             color: #2c3e50; /* Fallback color */
             -webkit-text-fill-color: initial; /* Reset gradient for small text */
        }}

        .metric-card .label {{
            color: #495057;
            margin-top: 8px;
            font-size: 0.9em;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }}

        .metric-card .sub-label {{
            color: #6c757d;
            font-size: 0.8em;
            margin-top: 6px;
            font-weight: 400;
        }}

        .comparison-container {{
            padding: 40px;
        }}

        .image-container {{
            display: flex;
            gap: 30px;
            justify-content: center;
            align-items: flex-start;
            min-height: 400px;
        }}

        .image-wrapper {{
            flex: 1;
            text-align: center;
            max-width: 50%;
        }}

        .image-wrapper h3 {{
            margin-bottom: 20px;
            color: #2c3e50;
            font-size: 1.6em;
            font-weight: 700;
            padding: 15px;
            background: linear-gradient(135deg, #f8f9fa 0%, #e9ecef 100%);
            border-radius: 10px;
            border-left: 5px solid #667eea;
        }}

        .image-wrapper img {{
            max-width: 100%;
            height: auto;
            border: 3px solid #dee2e6;
            border-radius: 12px;
            background: white;
            box-shadow: 0 10px 30px rgba(0,0,0,0.1);
            transition: all 0.3s ease;
        }}

        .image-wrapper img:hover {{
            transform: scale(1.02);
            box-shadow: 0 15px 40px rgba(0,0,0,0.15);
        }}

        .no-data {{
            text-align: center;
            padding: 80px 40px;
            color: #6c757d;
            font-size: 1.4em;
            font-weight: 500;
        }}

        .footer {{
            padding: 30px;
            text-align: center;
            background: linear-gradient(135deg, #2c3e50 0%, #34495e 100%);
            color: #ecf0f1;
            font-size: 1em;
        }}

        @media (max-width: 1200px) {{
            .metrics-grid {{
                grid-template-columns: repeat(2, 1fr);
            }}
        }}

        @media (max-width: 768px) {{
            .metrics-grid {{
                grid-template-columns: 1fr;
            }}
             .image-container {{
                flex-direction: column;
            }}
            .image-wrapper {{
                max-width: 100%;
            }}
        }}
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🔒 Chakravyuha Obfuscation Report</h1>
            <p>Visual Comparison of Original vs. Obfuscated Control Flow Graphs</p>
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
                        <div class="value">${{attrs.totalIRSizeChange || 'N/A'}}</div>
                        <div class="label">Total IR Size Change</div>
                        <div class="sub-label">${{attrs.originalIRSize || '?'}} &rarr; ${{attrs.obfuscatedIRSize || '?'}}</div>
                    </div>
                    <div class="metric-card">
                        <div class="value">${{bin.change_pct || 'N/A'}}</div>
                        <div class="label">Executable Size Change</div>
                        <div class="sub-label">${{bin.original_size || '?'}} &rarr; ${{bin.obfuscated_size || '?'}}</div>
                    </div>
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
                    </div>
                    <div class="metric-card">
                        <div class="value">${{fci.insertedBlocks || 0}}</div>
                        <div class="label">Fake Blocks Inserted</div>
                    </div>
                    <div class="metric-card">
                        <div class="value small-text">${{passes}}</div>
                        <div class="label">Passes Run</div>
                    </div>
                    <div class="metric-card">
                        <div class="value">${{attrs.stringDataSizeChange || '0.00%'}}</div>
                        <div class="label">IR String Data Change</div>
                        <div class="sub-label">(0% is expected)</div>
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
                print(
                    f"macos detected. prepending homebrew llvm to path: {llvm_bin_dir}"
                )
                os.environ["PATH"] = str(llvm_bin_dir) + os.pathsep + os.environ["PATH"]
        except (subprocess.CalledProcessError, FileNotFoundError):
            print(
                "warning: could not find homebrew llvm. "
                "assuming 'opt' and 'dot' are in the standard path."
            )

    parser = ArgumentParser(
        description="chakravyuha obfuscator visualization pipeline",
        formatter_class=RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "action",
        nargs="?",
        default="visualize",
        choices=["visualize", "view"],
        help="action to perform (default: visualize). 'view' opens the report in a browser.",
    )
    args = parser.parse_args()

    if generate_visualizations():
        report_path = create_comparison_html()
        if args.action == "view":
            view_report(report_path)


if __name__ == "__main__":
    main()
