# WuBu Hermes — C Translation

**Translating Hermes Agent from Python into C for zero-dependency operation.**

This directory (`C/`) lives inside the standard Hermes Agent repo and contains
the complete C translation. It tracks alongside the Python source so that
`git pull` updates both languages in one operation.

## Why

- **No Python runtime.** No pip, no venv, no virtualenv, no dependency hell.
- **Single binary.** `hermes` is one file you can scp to any Linux box.
- **Maximum performance.** C has no GIL, no interpreter overhead, no GC pauses.
- **Adoption path for upstream.** If the Nous team wants to use our fork,
  the C/ directory is a complete, reviewable feature branch.

## Directory Layout

```
C/
├── README.md              ← This file. Workflow overview.
├── DEPENDENCIES.md        ← Full Python → C dependency map (phase-ordered)
├── digestion.md           ← How the digestion system works
├── digest.py              ← Digestion automation script
├── Makefile               ← Build system (phase-ordered targets)
├── src/                   ← C source files, mirrors Python repo layout
│   ├── main.c             ← Entry point
│   ├── agent/             ← Agent loop, context, memory, skills
│   ├── cli/               ← CLI, config, display, commands
│   ├── gateway/           ← Gateway server + platform adapters
│   ├── tools/             ← Tool implementations
│   ├── cron/              ← Scheduler + job management
│   └── deps/              ← C equivalents of Python dependencies
├── include/
│   ├── hermes.h           ← Master header (types, constants, function decls)
│   └── *.h                ← Module-specific headers
└── scripts/
    └── build.sh           ← Build helper
```

## Workflow

### Daily Development

```bash
# 1. Pull latest from our fork (Python + C changes together)
cd ~/hermes-agent-dev
git pull wubu main

# 2. Run digestion to see what Python changed and what C needs updating
python3 C/digest.py

# 3. If digestion shows C work is needed:
#    - Edit C/src/*.c and C/include/*.h
#    - Build to verify
make -C C phase3   # or phase1/2/4/5 for specific tier

# 4. Commit both Python and C changes together
git add -A
git commit -m "feat(x): description + C translation"
git push wubu HEAD
```

### Creating Feature Branches

Each feature gets its own branch with BOTH Python and C implementation:

```bash
git checkout -b feat/some-feature wubu/main
# ... make Python changes ...
# ... make C/ translation changes ...
git push wubu feat/some-feature
# Open PR from feat/some-feature → wubu/main on GitHub
```

### C Translation Feature (special case)

```bash
git checkout -b feat/c-translation wubu/main
# C/ directory work only, no Python changes
git push wubu feat/c-translation
```

## Production Hermes (Daily Driver)

The production installation at `~/.hermes/hermes-agent/` is a SEPARATE git checkout.
It stays on `wubu/main` and is used for everyday work.

```bash
# Update production Hermes (Python only)
cd ~/.hermes/hermes-agent
git pull wubu main
# hermes command picks up changes automatically (pip -e install)
```

## Build Phases

The translation is organized into 5 phases. Each phase builds on the previous:

| Phase | What | make target | Deps needed |
|-------|------|-------------|-------------|
| 1 | Foundation: JSON, YAML, HTTP, SQLite, crypto wrappers | `phase1` | libsqlite3, libcurl, libyaml, openssl |
| 2 | Agent Core: loop, LLM client, CLI, config | `phase2` | Phase 1 |
| 3 | Tools: terminal, file, web, skills | `phase3` | Phase 2 |
| 4 | Gateway: server + platform adapters | `phase4` | Phase 3 |
| 5 | Cron + Advanced features | `phase5` / `all` | Phase 4 |

## What's Tracked vs Unchanged

- **`C/` directory** is tracked by git — all C code, build scripts, and docs.
- **Python source** in the repo root is NEVER modified by the C translation.
  The C/ directory is a parallel implementation.
- **`hermes update`** (the Hermes CLI command) updates the production Python install.
  It does NOT touch the C/ directory — that's managed via `git pull` in the dev clone.

## Developer Setup

```bash
# Install C dependencies
sudo apt install libsqlite3-dev libcurl4-openssl-dev libyaml-dev libssl-dev ncurses-dev

# Build phase 1 (foundation deps — verify toolchain works)
make -C C phase1

# Build full binary
make -C C

# Run digestion (after any git pull)
python3 C/digest.py
```
