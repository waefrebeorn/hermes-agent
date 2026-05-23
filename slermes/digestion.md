# Digestion System — How Updates Flow into C

## Concept

The digestion system tracks Python code changes from upstream Hermes Agent and maps them to C translation tasks. It:

1. **Fetches** origin/main to detect new Python commits
2. **Diffs** changed Python files since last sync
3. **Maps** each changed file to its C equivalent via the FILE_MAP in `digest.py`
4. **Reports** what C work is needed — by module, by phase

## Workflow

```bash
cd /home/wubu/hermes-agent-dev/C/

# Check for upstream changes:
python3 digest.py --upstream

# If changes found, merge them:
python3 digest.py --upstream --merge

# Manual diff of specific commits:
python3 digest.py --diff HEAD~5..HEAD

# Report what C files need updates:
python3 digest.py --modules agent/file_safety.py tools/browser_camofox.py
```

## FILE_MAP

The `digest.py` FILE_MAP has 250+ entries mapping Python modules to C files:

```
"agent/anthropic_adapter.py" → ("agent/provider_anthropic", "Anthropic provider")
"tools/terminal_tool.py"     → ("tools/terminal", "Terminal execution")
"gateway/run.py"             → ("gateway/server", "Gateway server")
```

Entries with `(".")` indicate the Python module has NO C equivalent yet — these are gaps tracked in the 300-gap roadmap.

## Super Fork Architecture

The C code lives in the SAME repo as the Python code (under `C/`). This means:
- `git pull wubu main` updates both Python and C in one operation
- `git fetch origin` fetches upstream Python changes
- `digest.py --upstream` detects what Python changes need C porting

## Current Status

- **Remote:** origin = NousResearch/hermes-agent (upstream)
- **Last sync:** `729a778af0b3` (May 2026)
- **Behind:** 183 commits
- **FILE_MAP coverage:** ~250 of 392 core Python files mapped
