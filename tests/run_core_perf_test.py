"""Compile the real core modules, run regressions and report 5-run medians.

Use --baseline-revision HEAD to compare with source from that Git revision.
"""
import argparse
import json
from pathlib import Path
import re
import statistics
import subprocess


def measure(executable, root):
    samples = {}
    for _ in range(5):
        output = subprocess.check_output([str(executable)], cwd=root, text=True)
        if "Core performance regressions: PASS" not in output:
            raise RuntimeError(output)
        for name, iterations, allocations, milliseconds in re.findall(
            r"(\w+) iterations=(\d+) allocations=(\d+) ms=([\d.]+)", output
        ):
            samples.setdefault(name, []).append((int(iterations), int(allocations), float(milliseconds)))
    return {
        name: {"iterations": values[0][0], "allocations": values[0][1],
               "median_ms": statistics.median(value[2] for value in values)}
        for name, values in samples.items()
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", default="g++", help="GCC/Clang compiler executable")
    parser.add_argument("--baseline-executable", type=Path)
    parser.add_argument("--baseline-revision", help="Compile a Git revision with the same workload")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = root / "build/core-perf"
    output.mkdir(parents=True, exist_ok=True)
    executable = output / "core_perf_test.exe"
    command = [args.compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Wno-mismatched-new-delete",
               "-Isrc", "tests/core_perf_test.cpp", "src/runtime/coroutine/coroutine_center.cpp",
               "src/runtime/coroutine/coroutine.cpp", "-o", str(executable)]
    subprocess.run(command, cwd=root, check=True)
    report = {"after": measure(executable, root)}
    if args.baseline_revision:
        baseline = output / "baseline"
        paths = ("src/runtime/event/event_channel.hpp", "src/runtime/reactive/reactive_value.hpp",
                 "src/runtime/reactive/dependency.hpp", "src/runtime/coroutine/coroutine_center.h",
                 "src/runtime/coroutine/coroutine_center.cpp")
        for path in paths:
            saved = baseline / path
            saved.parent.mkdir(parents=True, exist_ok=True)
            saved.write_bytes(subprocess.check_output(["git", "show", f"{args.baseline_revision}:{path}"], cwd=root))
        before_executable = output / "core_perf_before.exe"
        before_command = command.copy()
        before_command[before_command.index("-Isrc"):before_command.index("-Isrc")] = [
            "-DMEOWODY_BASELINE", f"-I{baseline / 'src'}"]
        before_command[before_command.index("src/runtime/coroutine/coroutine_center.cpp")] = str(
            baseline / "src/runtime/coroutine/coroutine_center.cpp")
        before_command[-1] = str(before_executable)
        subprocess.run(before_command, cwd=root, check=True)
        args.baseline_executable = before_executable
    if args.baseline_executable:
        report["before"] = measure(args.baseline_executable.resolve(), root)
        report["reduction_percent"] = {
            name: round(100 * (1 - result["median_ms"] / report["before"][name]["median_ms"]), 2)
            for name, result in report["after"].items()
            if name in report["before"]
        }
    (output / "results.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
