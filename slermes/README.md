# slermes/ — WuBu Hermes C Translation (Standalone Build Tree)

**Origin:** This is a standalone copy of the C translation worktree, kept alongside the main repo for parallel development. The canonical C source lives in `C/` at the repo root — this directory mirrors it.

## Current State

- **30 library archives** (`.a`) under `lib/` — JSON, YAML, HTTP, crypto, cron, path, datetime, CSV, hash, UUID, base64, HTML, textwrap, glob, signal, enum, difflib, regex, json5, websocket, toml, ansi, proc, tui, db, plugin, skin, template, mcp, protobuf
- **147 source files** under `src/`: agent loop, CLI with ~148 commands, 9 providers, 28 tools, 19 gateway platforms, cron scheduler
- **10 plugin backends** (`.so`): kanban, honcho, spotify, disk-cleanup, file-memory, achievements, observability, skills, image_gen, google_meet
- **116 test files**, suite **154/0/0**
- **Binary:** `hermes` — 9.2M dynamically linked, `make -j$(nproc)` to build

## Build

```bash
make -j$(nproc)        # Full binary
bash test_runner.sh    # 154 tests
```

## What's Here

```
slermes/
├── Makefile             # Phase-ordered build (phase1-libs → phase5-full)
├── test_runner.sh       # Test harness
├── digest.py            # Python-upstream-diff tracker (250+ FILE_MAP entries)
├── digestion.md         # How the digestion system works
├── README.md            # This file
├── DEPENDENCIES.md      # Python→C dependency map
├── GAP_ANALYSIS.md      # Legacy gap analysis (stale — see .hermes/mind-palace/plans/)
├── .gitignore
├── .digest_state.json   # Digestion state tracker
├── include/             # Public headers (hermes.h master header)
├── src/                 # C source — agent, cli, cron, gateway, tools, plugins
├── lib/                 # 30 standalone library archives
├── tests/               # 116 test files
├── scripts/             # Build helpers
├── examples/            # Usage examples
├── deps-repo/           # Pinned dependency sources (sqlite3.c, linline, etc.)
└── .hermes/             # Mind-palace: state.md, plans, DA audits, vault
```

## Key Difference from C/ Root

The `slermes/` tree is a **standalone build** — it has its own `Makefile`, `test_runner.sh`, and `lib/` that can be built independently of the repo root. The repo-root `C/` directory is a symlink pointing to `README.md` only; actual builds use this tree.

For workflow details, see `digestion.md`. For current gap tracking, see `.hermes/mind-palace/plans/300-gap-roadmap-v1.md`.
