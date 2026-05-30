#!/usr/bin/env python3
"""Performance gate: check binary size and startup time against baseline.

Usage: python3 scripts/perf-gate.py [--update-baseline]

Checks:
  - Binary size within 10% of baseline
  - Startup time within 500ms of baseline
  - Pass/fail exit code for CI

On first run (no baseline), saves current measurements as baseline.
"""

import json
import os
import subprocess
import sys
import time

CDIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASELINE_PATH = os.path.join(CDIR, ".perf-baseline.json")
HERMES_PATH = os.path.join(CDIR, "hermes")
UPDATE_BASELINE = "--update-baseline" in sys.argv


def get_binary_size():
    return os.path.getsize(HERMES_PATH)


def measure_startup():
    """Measure hermes --help startup time (ms)."""
    # Warmup
    subprocess.run([HERMES_PATH, "--help"], capture_output=True, timeout=10)
    # Measured run
    t0 = time.perf_counter()
    subprocess.run([HERMES_PATH, "--help"], capture_output=True, timeout=10)
    t1 = time.perf_counter()
    return int((t1 - t0) * 1000)


def load_baseline():
    if os.path.exists(BASELINE_PATH):
        with open(BASELINE_PATH) as f:
            return json.load(f)
    return None


def save_baseline(sz, startup_ms):
    baseline = {
        "binary_size_bytes": sz,
        "binary_size_human": f"{sz / 1024 / 1024:.1f}MB",
        "startup_ms": startup_ms,
        "startup_command": "--help",
        "baseline_date": time.strftime("%Y-%m-%d"),
        "tolerance": {
            "binary_size_percent": 10,
            "startup_ms_absolute": 500,
        },
    }
    with open(BASELINE_PATH, "w") as f:
        json.dump(baseline, f, indent=2)
    print(f"Baseline saved to {BASELINE_PATH}")
    return baseline


def fmt_size(b):
    return f"{b / 1024 / 1024:.1f}MB ({b} bytes)"


def main():
    if not os.path.exists(HERMES_PATH):
        print(f"FAIL: {HERMES_PATH} not found. Build first.")
        sys.exit(1)

    sz = get_binary_size()
    startup_ms = measure_startup()
    print(f"Binary: {fmt_size(sz)}")
    print(f"Startup: {startup_ms}ms")

    baseline = load_baseline()

    if baseline is None or UPDATE_BASELINE:
        save_baseline(sz, startup_ms)
        print("PASS (baseline established)")
        sys.exit(0)

    # Check against baseline
    tol_pct = baseline.get("tolerance", {}).get("binary_size_percent", 10)
    tol_ms = baseline.get("tolerance", {}).get("startup_ms_absolute", 500)
    b_sz = baseline["binary_size_bytes"]
    b_ms = baseline.get("startup_ms", 0)

    max_size = int(b_sz * (1 + tol_pct / 100.0))
    max_startup = b_ms + tol_ms

    ok = True

    if sz > max_size:
        print(f"FAIL: binary size {fmt_size(sz)} exceeds {fmt_size(max_size)}")
        print(f"       (baseline: {fmt_size(b_sz)}, tolerance: {tol_pct}%)")
        ok = False
    else:
        print(f"OK: binary size {fmt_size(sz)} (limit {fmt_size(max_size)})")

    if startup_ms > max_startup:
        print(f"FAIL: startup {startup_ms}ms exceeds {max_startup}ms")
        print(f"       (baseline: {b_ms}ms, tolerance: {tol_ms}ms)")
        ok = False
    else:
        print(f"OK: startup {startup_ms}ms (limit {max_startup}ms)")

    if ok:
        print("PASS: performance gate")
        sys.exit(0)
    else:
        print("FAIL: performance regression detected")
        sys.exit(1)


if __name__ == "__main__":
    main()
