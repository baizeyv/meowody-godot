"""Measure the actual reflection scan and hash emitted bindings for equivalence."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from reflect_codegen import reflect_generator as gen


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--samples", type=int, default=3)
    parser.add_argument("--output", type=Path, default=ROOT / "build/reflect-perf.json")
    args = parser.parse_args()
    headers = sorted(p for p in (ROOT / "src").rglob("*")
                     if p.is_file() and p.suffix in (".h", ".hpp", ".hh", ".hxx") and p.parent.name != "reflection")
    timings = []
    digest = None
    for _ in range(args.samples):
        start = time.perf_counter()
        classes = gen.build_model(gen.scan_headers(ROOT / "src", ROOT / "godot-cpp", ROOT / "build", headers))
        unique = {item["qualified"]: item for item in classes}
        emitted = "\n".join(gen.render_class(item) for _, item in sorted(unique.items()))
        current = hashlib.sha256(emitted.encode()).hexdigest()
        if digest is not None and current != digest:
            raise RuntimeError("Reflection output changed between samples")
        digest = current
        timings.append(time.perf_counter() - start)
    result = {"headers": len(headers), "classes": len(unique), "samples": timings,
              "median_seconds": statistics.median(timings), "bindings_sha256": digest}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, indent=2), flush=True)


if __name__ == "__main__":
    main()
