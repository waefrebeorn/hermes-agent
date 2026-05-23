# Overnight Map — 2026-05-23 (DA v11)

## Active: Stub Hunt + CI Fix + Battleship 500

**Suite: 154/0/0** (117 tests, ~573 assertions)
**Binary: 9.1MB dynamic**
**400 commits, 0 behind upstream**

## What Was Done (May 23)
- **Stub hunt**: Found 4 verified stubs (computer_use 100%, CDP browser 5/11, image_gen fake URLs, TUI placeholder)
- **False ✅ corrected**: "28 tools"→68, "32 .a"→32 lib units
- **CI fixed**: Docker wrong WORKDIR → `cd C && make`. Un-truncated build logs. No `make clean` before TUI.
- **Battleship v3**: 468→500 gaps. 3 new sectors: Stubs (10), Tests (12), CI/CD (10).
- **README fixed**: Root is now symlink→C/README.md (was backwards). Root shows C project content.
- **Branches cleaned**: Deleted stale `wubu` branch on fork. Only `main` remains.
- **Dockerfile fixed**: `RUN make` at /build (no Makefile) → `cd C && make`. Stage2 COPY paths corrected.

## P0 Gaps (next session picks first)
1. **S01-S03** — computer_use real backend (5 broken tool registrations)
2. **U01-U02** — CI gate + Docker build verification
3. **S04-S06** — CDP browser WebSocket client (5 broken tools)

## Verified Numbers (don't re-derive)
- Parity: 32% (161/500), NOT 36% or 34%
- Tools: 68 registered (NOT 28)
- Libraries: 30 units compiled into binary (no .a files)
- Stubs: 4 critical (computer_use, CDP, image_gen, TUI)
- CI workflows: c-build.yml (Linux + Docker) — nix.yml failures are Python infra, not C
- Branches: only `main` on fork. `wubu` branch deleted.

## Fallback
Pick S01 (computer_use MCP client) — highest-impact broken feature.
