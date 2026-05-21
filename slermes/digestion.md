# Digestion System — How Updates Flow into C

## Concept

The digestion system tracks Python code changes from upstream Hermes Agent updates
and maps them to C translation tasks. It does NOT auto-translate (that's impossible
for arbitrary Python). Instead, it:

1. **Detects** what Python files changed after `git pull`
2. **Maps** each changed file to its C equivalent via `DEPENDENCIES.md`
3. **Analyzes** the Python diff to extract function signatures, class interfaces,
   and new logic paths
4. **Generates** C stub headers with the new interface, flagging what needs
   manual translation
5. **Reports** what's needed — ordered by translation phase

## Workflow

```
1. git pull wubu main           ← get latest Hermes + C work from wubu fork
2. python3 C/digest.py          ← run digestion on the diff
3. Review C/digest_output/      ← see what C files need updates
4. Translate flagged items       ← manual C implementation
5. make -C C test               ← verify C builds
6. git add -A && git commit     ← commit C changes alongside Python changes
7. git push wubu HEAD           ← push everything together
```

The C code lives in the SAME repo as the Python code (under `C/`). This means:
- `git pull` updates BOTH Python and C in one operation
- The C translation is always in sync with the Python version it translates
- Feature branches contain both Python and C changes for the same feature

## Digest Script Usage

```bash
# Full digestion: compare C/ against current Python codebase
python3 C/digest.py

# Digest only specific modules
python3 C/digest.py --modules agent/title_generator.py tools/file_tools.py

# Digest against a specific diff (e.g., after pull shows new commits)
python3 C/digest.py --diff HEAD~5..HEAD

# Output format: summary (default), json, or detailed
python3 C/digest.py --format json

# Generate C stub headers for flagged items
python3 C/digest.py --generate-stubs
```

## How It Maps Files

```
Python File                    →  C File
──────────────────────────────────────────
run_agent.py                   →  src/agent/agent_loop.c + .h
cli.py                         →  src/cli/cli.c + .h
agent/title_generator.py      →  src/agent/title.c + .h
tools/terminal_tool.py        →  src/tools/terminal.c + .h
tools/file_tools.py           →  src/tools/file.c + .h
gateway/run.py                →  src/gateway/server.c + .h
gateway/platforms/telegram.py →  src/gateway/platforms/telegram.c + .h
cron/scheduler.py             →  src/cron/scheduler.c + .h
deps/httpx/*                  →  src/deps/http.c + .h
deps/pydantic/*               →  src/deps/json.c + .h
```

## Invariants

- **Never modify Python code in C/.** Python stays in the repo root as-is.
  C/ contains only C translation of that Python.
- **A `git pull` that changes Python ALWAYS needs a digestion run.**
  Run `digest.py` after every pull before starting C work.
- **Phase order is enforced.** Phase 1 (deps) must build before Phase 2 (agent core).
  The digest script only flags work in the current or next phase.
- **C code commits alongside Python code.** A feature branch has both
  `tools/file_tools.py` changes AND `C/src/tools/file.c` changes in the same PR.
