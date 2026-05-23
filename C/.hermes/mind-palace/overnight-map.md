# Overnight Map — 2026-05-23

## Active: Upstream Sync + DA v10 + Battleship v2

**Suite: 154/0/0**
**Binary: 9.2M dynamically linked**
**400 commits, 0 behind upstream**

## What Was Done (May 23 — late session)

- **Upstream sync**: 183 commits merged from origin/main via `git merge origin/main`. 313 files changed (17K lines). 0 behind.
- **Triple DA v10**: Source-audited all 115 .c files, 117 tests, 32 libs, 10 plugins, 19 gateway platforms. Real parity: 34% (161/468).
- **Battleship v2**: Expanded from 447→468 gaps (21 new from upstream). Written to `plans/battleship-v2.md`.
- **New gaps catalogued**: computer_use backends (5 files: backend.py, cua_backend.py, schema.py, tool.py, vision_routing.py), Fal AI (fal_common.py, fal image/video plugins), Discord platform plugin, secrets subsystem (2 files), parallel test runner, transcription tools, SSH environment, MCP OAuth (2 files), OpenViking memory plugin
- **55 modified files need C review** — existing C equivalents may be out of sync with upstream
- **All docs updated**: battleship-index.md, state.md, prestige_prompt.md (v11), overnight-map.md

## P0 Gaps (next session picks first)

1. **D75-D79** — computer_use backends (5 new upstream files — backend, cua_backend, schema, tool, vision_routing)
2. **D81-D83** — skill_manager_tool, skill_usage, transcription_tools
3. **D84** — SSH environment backend

## Data Not to Re-Derive

- Real parity: 34% (161/468), NOT 69% or 36%
- Library count: 32 .a (was 30)
- Source files: 115 .c + 29 .h = 144
- Test files: 117 (was 116)
- Suite: 154/0/0
- Commits: 400 C-specific (was 396)
- Upstream: caught up (0 behind)
- Bugs: 16 total (see vault/bug-bounty.md)
- New gaps: 21 from upstream merge

## Fallback
Pick D75 (computer_use backend) — it's the highest-impact unported upstream feature.
