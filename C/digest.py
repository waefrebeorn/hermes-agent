#!/usr/bin/env python3
"""
digest.py — WuBu Hermes C Translation Digestion Engine

Analyzes Python code changes and maps them to C translation tasks.
Supports upstream tracking for the Super Fork: automatically detect
new Python changes from upstream and flag what C work is needed.

Usage:
  python3 digest.py                           # Diff last commit
  python3 digest.py --diff HEAD~5..HEAD       # Custom range
  python3 digest.py --modules tools/terminal.py tools/file_tools.py  # Manual

  # Super Fork upstream tracking:
  python3 digest.py --upstream                # Fetch origin/main, diff, report
  python3 digest.py --upstream --merge        # Fetch + merge into wubu/main
"""
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

# File mapping: Python module → C file
FILE_MAP = {
    "run_agent.py":                          ("agent/agent_loop", "Core agent conversation loop"),
    "cli.py":                                ("cli/cli", "CLI orchestrator"),
    "model_tools.py":                        ("agent/tool_dispatch", "Tool orchestration"),
    "toolsets.py":                           ("agent/tool_dispatch", "Toolset definitions"),
    "hermes_state.py":                       ("deps/db", "File-based session store"),
    "hermes_constants.py":                   (".", "Constants (inline in hermes.h)"),
    "hermes_logging.py":                     (".", "Logging (inline or syslog)"),
    "tools/registry.py":                     ("tools/registry", "Tool registry"),
    "tools/terminal_tool.py":                ("tools/terminal", "Terminal execution"),
    "tools/file_tools.py":                   ("tools/file", "File I/O tools"),
    "tools/web_tools.py":                    ("tools/web", "Web search tools"),
    "tools/skills_tool.py":                  ("tools/skills", "Skill management"),
    "tools/skills_guard.py":                 ("tools/skills_guard", "Skills security guard"),
    "gateway/run.py":                        ("gateway/server", "Gateway server"),
    "gateway/platforms/telegram.py":         ("gateway/platforms/telegram", "Telegram adapter"),
    "gateway/platforms/discord.py":          ("gateway/platforms/discord", "Discord adapter"),
    "cron/scheduler.py":                     ("cron/scheduler", "Cron scheduler"),
    "cron/jobs.py":                          ("cron/jobs", "Job management"),
    "agent/title_generator.py":              ("agent/title", "Title generation"),
    "agent/display.py":                      ("cli/display", "Terminal display"),
    "agent/context.py":                      ("agent/context", "Context management"),
    "agent/compression.py":                  ("agent/compress", "Context compression"),
    "agent/memory.py":                       ("agent/memory", "Memory system"),
    "agent/skill_commands.py":               ("agent/skills", "Skill commands"),
    "hermes_cli/main.py":                    ("cli/main", "CLI entry point"),
    "hermes_cli/config.py":                  ("cli/config", "Config loading"),
    "hermes_cli/commands.py":                ("cli/commands", "Slash commands"),
}

# Phase ordering
PHASES = {
    1: "Foundation (deps)",
    2: "Agent Core",
    3: "Tools",
    4: "Gateway",
    5: "Cron + Advanced",
}

PHASE_MAP = {
    "deps/": 1,
    "agent/agent_loop": 2,
    "agent/llm_client": 2,
    "agent/context": 2,
    "cli/main": 2,
    "cli/cli": 2,
    "cli/display": 2,
    "tools/": 3,
    "gateway/": 4,
    "cron/": 5,
    "agent/": 5,
}

HERMES_HOME = Path(__file__).resolve().parent.parent  # C/.. = repo root
C_DIR = HERMES_HOME / "C"
DEP_MD = C_DIR / "DEPENDENCIES.md"
DIGEST_STATE = C_DIR / ".digest_state.json"


def git(*args, cwd=HERMES_HOME, check=True, capture=True, timeout=15):
    """Run git command, return stdout string or CompletedProcess."""
    result = subprocess.run(
        ["git"] + list(args), capture_output=capture, text=True,
        cwd=cwd, timeout=timeout, check=check
    )
    if capture:
        return result.stdout.strip()
    return result


def get_changed_files_since(ref="HEAD~1..HEAD"):
    """Get list of changed Python files since git ref."""
    try:
        result = git("diff", "--name-only", ref, "--", "*.py")
        return [f for f in result.split("\n") if f]
    except Exception:
        return []


def get_diff(ref="HEAD~1..HEAD"):
    """Get full diff text for analysis."""
    try:
        return git("diff", ref, "--", "*.py")
    except Exception:
        return ""


def extract_functions_from_diff(diff_text):
    """Extract new/modified function signatures from diff."""
    funcs = []
    for line in diff_text.split("\n"):
        m = re.match(r'^\+.*def (\w+)\(([^)]*)\)', line)
        if m:
            funcs.append(f"{m.group(1)}({m.group(2)})")
    return funcs


def extract_classes_from_diff(diff_text):
    """Extract new/modified classes from diff."""
    classes = []
    for line in diff_text.split("\n"):
        m = re.match(r'^\+.*class (\w+)\(?([^)]*)\)?\s*:', line)
        if m:
            classes.append({"name": m.group(1), "bases": m.group(2)})
    return classes


def extract_imports_from_diff(diff_text):
    """Extract new imports added in diff."""
    imports = []
    for line in diff_text.split("\n"):
        m = re.match(r'^\+.*(?:import|from)\s+(\S+)', line)
        if m:
            imports.append(m.group(1))
    return imports


def map_to_c_file(py_file):
    """Map a Python file path to its C translation path."""
    py_file = py_file.replace("\\", "/")

    for pattern, (c_path, desc) in FILE_MAP.items():
        if py_file == pattern or py_file.endswith("/" + pattern):
            return c_path, desc

    stem = Path(py_file).stem
    parent = Path(py_file).parent

    if str(parent) == ".":
        return f"agent/{stem}", f"Module: {stem}"

    first_dir = str(parent).split("/")[0]
    if first_dir in ("tools", "gateway", "cron", "agent", "hermes_cli", "plugins"):
        return f"{first_dir}/{stem}", f"Module: {stem}"

    return f"unknown/{stem}", f"Unmapped: {py_file}"


def get_phase(c_path):
    """Determine translation phase for a C path."""
    for prefix, phase in sorted(PHASE_MAP.items(), key=lambda x: -len(x[0])):
        if c_path.startswith(prefix) or c_path == prefix.rstrip("/"):
            return phase
    return 5


def load_translation_status():
    """Load current translation status from DEPENDENCIES.md."""
    if not DEP_MD.exists():
        return {}
    status = {}
    content = DEP_MD.read_text()
    for line in content.split("\n"):
        parts = [p.strip() for p in line.split("|")]
        if len(parts) >= 5:
            name = parts[1].strip("`")
            c_eq = parts[3].strip("`")
            stat = parts[4].strip()
            if name and stat in ("🔲", "🔄", "✅"):
                status[name] = {"c_equiv": c_eq, "status": stat}
    return status


def generate_report(changed_files, funcs, classes, imports):
    """Generate a structured report of what needs translation."""
    report = {
        "summary": {
            "python_files_changed": len(changed_files),
            "functions_detected": len(funcs),
            "classes_detected": len(classes),
            "new_imports": len(imports),
        },
        "changed_files": [],
        "work_items": [],
    }

    for py_file in changed_files:
        c_path, desc = map_to_c_file(py_file)
        phase = get_phase(c_path)

        entry = {
            "python": py_file,
            "c_file": f"src/{c_path}.c",
            "c_header": f"include/{c_path}.h" if c_path != "." else "include/hermes.h",
            "description": desc,
            "phase": phase,
            "phase_name": PHASES.get(phase, "Unknown"),
        }
        report["changed_files"].append(entry)

        c_exists = (C_DIR / "src" / f"{c_path}.c").exists()

        item = {
            "c_file": entry["c_file"],
            "status": "✅ up to date" if (c_exists and not funcs) else "🔄 needs update" if c_exists else "🔲 not started",
            "phase": entry["phase_name"],
            "action": "Implement" if not c_exists else "Update for new functions",
            "functions": funcs[:5],
            "classes": [c["name"] for c in classes[:3]],
        }
        report["work_items"].append(item)

    report["work_items"].sort(key=lambda x: x.get("phase_num", 99) or
                              [k for k, v in PHASES.items() if v == x["phase"]][0])

    return report


def print_report(report, title="Digestion Report"):
    """Print human-readable report."""
    s = report["summary"]
    print(f"\n{'═'*60}")
    print(f"  {title}")
    print(f"  {s['python_files_changed']} Python files changed")
    print(f"  Functions: {s['functions_detected']}  Classes: {s['classes_detected']}  Imports: {s['new_imports']}")
    print(f"{'═'*60}")

    if not report["changed_files"]:
        print("\n  No Python files changed since last digest.\n")
        return

    print(f"\n{'─'*60}")
    print(f"  Translation Work Items (by phase):")
    print(f"{'─'*60}")

    current_phase = None
    for item in report["work_items"]:
        if item["phase"] != current_phase:
            current_phase = item["phase"]
            pnum = [k for k, v in PHASES.items() if v == current_phase][0]
            print(f"\n  ▸ Phase {pnum}: {current_phase}")
            print(f"    {'─'*50}")

        status_icon = {"✅ up to date": "✅", "🔄 needs update": "🔄", "🔲 not started": "🔲"}
        icon = status_icon.get(item["status"], "❓")
        print(f"    {icon} {item['c_file']}")
        print(f"       {item['action']}")
        if item.get("functions"):
            print(f"       fn: {', '.join(item['functions'][:3])}")
        if item.get("classes"):
            print(f"       cls: {', '.join(item['classes'][:3])}")

    print()


# ─── Super Fork: Upstream Tracking ─────────────────────────────────────

def load_digest_state():
    """Load last sync state."""
    if DIGEST_STATE.exists():
        return json.loads(DIGEST_STATE.read_text())
    return {"last_sync_commit": None, "last_sync_time": None, "upstream_head": None}


def save_digest_state(state):
    """Save sync state."""
    DIGEST_STATE.write_text(json.dumps(state, indent=2))
    print(f"  [digest] state saved to {DIGEST_STATE}")


def upstream_sync(do_merge=False):
    """
    Super Fork upstream sync.
    1. Fetch origin/main (upstream)
    2. Find Python files changed since last sync
    3. Diff them, map to C work items
    4. Optionally merge upstream into current branch
    """
    state = load_digest_state()
    print(f"\n  [upstream] fetching origin/main...")
    git("fetch", "origin", "main", "--no-tags")

    # Get upstream head
    up_head = git("rev-parse", "origin/main")
    print(f"  [upstream] origin/main at {up_head[:12]}")

    last_sync = state.get("last_sync_commit")
    if last_sync:
        print(f"  [upstream] last sync was {last_sync[:12]}")
    else:
        # First sync: track all Python files
        print(f"  [upstream] first sync — full scan")
        last_sync = git("merge-base", "origin/main", "HEAD")

    # Diff Python files since last sync
    diff_range = f"{last_sync}..origin/main"
    print(f"  [upstream] diff range: {diff_range}")

    try:
        changed = get_changed_files_since(diff_range)
    except Exception as e:
        print(f"  [upstream] diff failed: {e}")
        changed = []

    # Check if there's any difference
    ahead = git("rev-list", "--count", f"{last_sync}..origin/main")
    print(f"  [upstream] {ahead.strip()} new commits on upstream")

    if not changed:
        print(f"  [upstream] no Python file changes since last sync\n")
        # Still update state so we track position
        state["last_sync_commit"] = up_head
        state["last_sync_time"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        state["upstream_head"] = up_head
        save_digest_state(state)
        print(f"\n{'='*60}")
        print(f"  ✅ Super Fork: No new C work needed — upstream Python unchanged")
        print(f"{'='*60}\n")
        return

    # Analyze diff
    diff_text = get_diff(diff_range)
    funcs = extract_functions_from_diff(diff_text)
    classes = extract_classes_from_diff(diff_text)
    imports = extract_imports_from_diff(diff_text)

    report = generate_report(changed, funcs, classes, imports)

    # Also list changed Python files nicely
    print(f"\n  [upstream] Changed Python files ({len(changed)}):")
    for f in changed:
        print(f"    • {f}")

    print_report(report, title="Super Fork: Upstream Change Digest")

    # Save sync state
    state["last_sync_commit"] = up_head
    state["last_sync_time"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    state["upstream_head"] = up_head
    state["last_changed_python"] = changed
    state["last_new_funcs"] = funcs
    state["last_new_classes"] = [c["name"] for c in classes]
    save_digest_state(state)

    # Generate JSON report for programmatic use
    report_path = C_DIR / "digest_output" / "upstream_report.json"
    report_path.parent.mkdir(exist_ok=True)
    report_path.write_text(json.dumps({
        "timestamp": state["last_sync_time"],
        "upstream_head": up_head,
        "changed_files": changed,
        "functions": funcs,
        "classes": [c["name"] for c in classes],
        "work_items": report["work_items"],
    }, indent=2))
    print(f"\n  [digest] JSON report saved to {report_path}")

    # Optionally merge upstream into current branch
    if do_merge:
        print(f"\n  [upstream] merging origin/main into current branch...")
        # Stash any local changes first
        git("stash", check=False, capture=False)
        try:
            git("merge", "origin/main", "-m", "chore: sync with upstream (auto-merge via digest.py)")
            print(f"  [upstream] merged successfully")
        except Exception as e:
            print(f"  [upstream] merge failed: {e}")
            print(f"  [upstream] run manually: git merge origin/main")
        git("stash", "pop", check=False, capture=False)

    return changed, funcs, classes, report


# ─── Stub Generation ────────────────────────────────────────────────────

def generate_stubs(report):
    """Generate C header stubs for unimplemented files."""
    stub_dir = C_DIR / "digest_output"
    stub_dir.mkdir(exist_ok=True)
    count = 0
    for item in report["work_items"]:
        if item["status"].startswith("🔲"):
            c_path = item["c_file"]
            stub_name = Path(c_path).name
            stub_file = stub_dir / stub_name
            if not stub_file.exists():
                guard = Path(c_path).stem.upper().replace("-", "_")
                stub_file.write_text(f"""#ifndef {guard}_H
#define {guard}_H

/* Auto-generated stub from Super Fork digestion.
 * Python: {item.get('python_file', 'unknown')}
 * Phase: {item.get('phase', 'unknown')}
 * Upstream trigger: origin/main
 */

// TODO: implement

#endif
""")
                count += 1
    print(f"  [digest] {count} stubs generated in {stub_dir}")
    return count


# ─── Main ───────────────────────────────────────────────────────────────

def main():
    import argparse
    parser = argparse.ArgumentParser(
        description="WuBu Hermes C Translation Digestion Engine — Super Fork",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 digest.py                              # Diff last commit
  python3 digest.py --diff HEAD~5..HEAD          # Custom range
  python3 digest.py --upstream                   # Fetch origin/main, diff, report
  python3 digest.py --upstream --merge           # Fetch + merge upstream
  python3 digest.py --upstream --generate-stubs  # Fetch + diff + stub new files
        """)
    parser.add_argument("--diff", default="HEAD~1..HEAD",
                        help="Git diff range to analyze")
    parser.add_argument("--modules", nargs="+",
                        help="Specific modules to digest")
    parser.add_argument("--format", choices=["summary", "json", "detailed"],
                        default="summary")
    parser.add_argument("--generate-stubs", action="store_true",
                        help="Generate C stub headers for unimplemented files")
    parser.add_argument("--upstream", action="store_true",
                        help="Super Fork mode: fetch origin/main, diff since last sync, report C work needed")
    parser.add_argument("--merge", action="store_true",
                        help="With --upstream: also merge upstream into current branch")
    args = parser.parse_args()

    if args.upstream:
        result = upstream_sync(do_merge=args.merge)
        if result:
            changed, funcs, classes, report = result
            if args.generate_stubs and report.get("work_items"):
                generate_stubs(report)
            if args.format == "json":
                print(json.dumps(report, indent=2))
        return

    if args.modules:
        changed_files = args.modules
        diff_text = ""
    else:
        changed_files = get_changed_files_since(args.diff)
        diff_text = get_diff(args.diff)

    funcs = extract_functions_from_diff(diff_text)
    classes = extract_classes_from_diff(diff_text)
    imports = extract_imports_from_diff(diff_text)

    report = generate_report(changed_files, funcs, classes, imports)

    if args.format == "json":
        print(json.dumps(report, indent=2))
    else:
        print_report(report)

    if args.generate_stubs:
        generate_stubs(report)


if __name__ == "__main__":
    main()
