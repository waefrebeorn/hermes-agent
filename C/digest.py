#!/usr/bin/env python3
"""
digest.py — WuBu Hermes C Translation Digestion Engine

Analyzes Python code changes and maps them to C translation tasks.
Run after every `git pull wubu main` to see what C work is needed.
"""
import json
import os
import re
import subprocess
import sys
from pathlib import Path

# File mapping: Python module → C file
FILE_MAP = {
    "run_agent.py":                          ("agent/agent_loop", "Core agent conversation loop"),
    "cli.py":                                ("cli/cli", "CLI orchestrator"),
    "model_tools.py":                        ("agent/tool_dispatch", "Tool orchestration"),
    "hermes_state.py":                       ("deps/db", "SQLite session store"),
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


def get_changed_files_since(ref="HEAD~1"):
    """Get list of changed Python files since git ref."""
    try:
        result = subprocess.run(
            ["git", "diff", "--name-only", ref, "--", "*.py"],
            capture_output=True, text=True, cwd=HERMES_HOME, timeout=10
        )
        return [f for f in result.stdout.strip().split("\n") if f]
    except Exception as e:
        print(f"  [warn] git diff failed: {e}", file=sys.stderr)
        return []


def get_diff(ref="HEAD~1..HEAD"):
    """Get full diff text for analysis."""
    try:
        result = subprocess.run(
            ["git", "diff", ref, "--", "*.py"],
            capture_output=True, text=True, cwd=HERMES_HOME, timeout=10
        )
        return result.stdout
    except Exception as e:
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
    # Normalize path
    py_file = py_file.replace("\\", "/")
    
    for pattern, (c_path, desc) in FILE_MAP.items():
        if py_file == pattern or py_file.endswith("/" + pattern):
            return c_path, desc
    
    # Fallback: derive from path
    stem = Path(py_file).stem
    parent = Path(py_file).parent
    
    if str(parent) == ".":
        return f"agent/{stem}", f"Module: {stem}"
    
    # Check if it's a tool or known subdir
    first_dir = str(parent).split("/")[0]
    if first_dir in ("tools", "gateway", "cron", "agent", "hermes_cli"):
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
    
    # Parse table rows
    for line in content.split("\n"):
        parts = [p.strip() for p in line.split("|")]
        if len(parts) >= 5:
            name = parts[1].strip("`")
            role = parts[2].strip("`")
            c_eq = parts[3].strip("`")
            stat = parts[4].strip()
            if name and stat in ("🔲", "🔄", "✅"):
                status[name] = {"role": role, "c_equiv": c_eq, "status": stat}
    
    return status


def mark_affected(translation_status, c_path, funcs, classes):
    """Mark dependencies as affected by this change."""
    affected = []
    c_stem = Path(c_path).stem if c_path != "." else "hermes"
    
    for name, info in translation_status.items():
        if c_stem.lower() in name.lower() or name.lower() in c_stem.lower():
            affected.append({
                "dep": name,
                "c_file": info["c_equiv"],
                "status": info["status"],
                "new_funcs": funcs,
                "new_classes": classes,
            })
    
    return affected


def generate_report(changed_files, affected_deps, funcs, classes, imports):
    """Generate a structured report of what needs translation."""
    report = {
        "summary": {
            "python_files_changed": len(changed_files),
            "functions_detected": len(funcs),
            "classes_detected": len(classes),
            "new_imports": len(imports),
            "deps_affected": len(affected_deps),
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
        
        # Determine status based on what exists
        c_exists = (C_DIR / "src" / f"{c_path}.c").exists()
        h_exists = (C_DIR / "include" / f"{c_path}.h").exists() or (C_DIR / "include" / "hermes.h").exists()
        
        item = {
            "c_file": entry["c_file"],
            "status": "✅ up to date" if (c_exists and not funcs) else "🔄 needs update" if c_exists else "🔲 not started",
            "phase": entry["phase_name"],
            "action": "Implement" if not c_exists else "Update for new functions",
            "functions": funcs[:5],  # top 5
            "classes": [c["name"] for c in classes[:3]],
        }
        report["work_items"].append(item)
    
    # Sort by phase
    report["work_items"].sort(key=lambda x: x.get("phase_num", 99) or 
                              [k for k, v in PHASES.items() if v == x["phase"]][0])
    
    return report


def print_report(report):
    """Print human-readable report."""
    s = report["summary"]
    print(f"\n{'═'*60}")
    print(f"  Digestion Report — {s['python_files_changed']} Python files changed")
    print(f"{'═'*60}")
    print(f"  Functions: {s['functions_detected']}  Classes: {s['classes_detected']}")
    print(f"  New imports: {s['new_imports']}  Deps affected: {s['deps_affected']}")
    print()
    
    if not report["changed_files"]:
        print("  No Python files changed since last digest.")
        print("  (Run with --diff to specify a different ref range)")
        return
    
    print(f"{'─'*60}")
    print(f"  Translation Work Items (by phase):")
    print(f"{'─'*60}")
    
    current_phase = None
    for item in report["work_items"]:
        if item["phase"] != current_phase:
            current_phase = item["phase"]
            print(f"\n  ▸ Phase {[k for k,v in PHASES.items() if v==current_phase][0]}: {current_phase}")
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


def main():
    import argparse
    parser = argparse.ArgumentParser(description="WuBu Hermes C Translation Digestion Engine")
    parser.add_argument("--diff", default="HEAD~1..HEAD", help="Git diff range to analyze")
    parser.add_argument("--modules", nargs="+", help="Specific modules to digest")
    parser.add_argument("--format", choices=["summary", "json", "detailed"], default="summary")
    parser.add_argument("--generate-stubs", action="store_true", help="Generate C stub headers")
    args = parser.parse_args()
    
    if args.modules:
        changed_files = args.modules
        diff_text = ""
    else:
        changed_files = get_changed_files_since(args.diff)
        diff_text = get_diff(args.diff)
    
    funcs = extract_functions_from_diff(diff_text)
    classes = extract_classes_from_diff(diff_text)
    imports = extract_imports_from_diff(diff_text)
    
    trans_status = load_translation_status()
    
    all_affected = []
    for py_file in changed_files:
        c_path, desc = map_to_c_file(py_file)
        affected = mark_affected(trans_status, c_path, funcs, classes)
        all_affected.extend(affected)
    
    report = generate_report(changed_files, all_affected, funcs, classes, imports)
    
    if args.format == "json":
        print(json.dumps(report, indent=2))
    else:
        print_report(report)
    
    if args.generate_stubs:
        stub_dir = C_DIR / "digest_output"
        stub_dir.mkdir(exist_ok=True)
        for item in report["work_items"]:
            if item["status"].startswith("🔲"):
                c_path = item["c_file"]
                stub_file = stub_dir / Path(c_path).name
                if not stub_file.exists():
                    guard = Path(c_path).stem.upper().replace("-", "_")
                    stub_file.write_text(f"""#ifndef {guard}_H
#define {guard}_H

/* Auto-generated stub from digestion.
 * Python: {item['python_file'] if 'python_file' in item else 'unknown'}
 * Phase: {item['phase']}
 */

// TODO: implement

#endif
""")
        print(f"  Stubs generated in {stub_dir}")


if __name__ == "__main__":
    main()
