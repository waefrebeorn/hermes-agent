# Overnight Map — 2026-05-23

## Active: Docs Restructure + Vault Reorg

**Suite: 154/0/0**
**Binary: 9.2M dynamically linked**
**396 commits, 183 behind upstream**

## What Was Done (May 23)

- **slermes/ removed** — stale duplicate of C/, 159 files deleted
- **All dates fixed**: CHANGELOG, essays, vault, mind-palace — May not June
- **Human time estimates removed**: no "18 months", "4 tests/week"
- **vault restructured**: bug-bounty.md (16 bugs), credits.md ($69.32 usage), legacy-plans-archive.md
- **Mind-palace cleaned**: legacy/ dir removed (14 files → 1 archive)
- **state.md, prestige_prompt.md, overnight-map.md** updated with v10 DA recalc
- **Root docs updated**: README (36% parity), slermes/DEPENDENCIES/GAP_ANALYSIS/digestion
- **Caveman skill updated** — C translation workflow patterns

## Data Not to Re-Derive

- Real parity: 36% (161/447), NOT 69% — DA v9 verified against actual Python source
- Bug count: 16 total (6 critical, 10 high) — see vault/bug-bounty.md
- Library count: 30 .a files
- Suite: 154/0/0 (116 files)
- Binary: 9.2M dynamically linked
- Upstream: 183 commits behind
- Project name: "slermes" is dead, code in C/

## P1 Gaps (next session)

1. Cron test coverage — cron_cli.c, jobs.c, scheduler.c, cron_locking.c (4 untested cron files)
2. Tool handler tests — browser.c, computer_use.c, discord.c, homeassistant.c, image_gen.c, registry.c, session_crud.c, tirith.c, tool_init.c, voice_mode.c (11 untested)
3. CLI/ACP server tests

## Fallback
Read plan.md and pick the next untested source file.
