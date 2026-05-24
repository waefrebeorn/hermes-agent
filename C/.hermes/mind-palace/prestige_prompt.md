# WuBu Hermes C — Prestige Prompt (2026-05-24 v15)

**Reset session — DA v15 fresh survey. Old battleship (500 gaps) archived. New battleship-v4 (~270 gaps).**

## Identity
1:1 C reimplementation of Python hermes-agent. ~76K LOC C source. 44 agent files, 57 libs, 83 tools, 9 providers, 19 gateways, 31 tool init functions, 173 tests. Synced upstream (740+ commits).

## Current State (2026-05-27 — code survey update)

| Metric | Value | Verification |
|--------|-------|-------------|
| Suite | ~213/0/0 | 173 test files (timeout at 120s) |
| Binary | 29MB ELF, 0 errors, 0 warnings | ✅ Verified |
| Tool registrations | ~83 across 31 init functions | ✅ All real, 0 stubs |
| Gateway platforms | 19 of 31 Python modules (61%) | ✅ C files exist |
| Providers | 9 native C of 28 Python plugins (32%) | ✅ All real |
| CLI commands | **79 real cmd_ in commands.c** (3702 lines) | ✅ **0 stubs** |
| Agent modules | 44 .c of 77 .py (57%) | 🔶 33 real ports done |
| ACP | 5 of 9 modules (56%) | ✅ Mostly done |
| Libraries | 57 lib/ directories | ✅ Clean compilation |
| Config | ~322 of 432 keys | 🟡 Unknown exact count |
| Stubs | **0 true stubs** | **S01/CDP fixed** |
| CLI vs Python | 8 .c files vs 88 .py modules | **Module depth gap, not cmd stubs** |
| Parity (corrected) | **~63%** | **~200 remaining gaps** |

## Priority Queue — Remaining Gaps (~270)

### P0 — Critical
| # | Sector | What | Why |
|---|--------|------|-----|
| 1 | C + Q | CLI depth — 88 Python modules, 197 stub commands | Biggest parity gap. 8 .c vs 88 .py |
| 2 | B | Agent modules — 35+ unported .py files | Core functionality missing |
| 3 | A | Core — agent_init.py, agent_runtime_helpers.py, conversation_loop.py | Agent lifecycle |

### P1 — High Impact
| # | Sector | What | Why |
|---|--------|------|-----|
| 1 | K | Provider plugins — 19 of 28 missing | User-facing, feature parity |
| 2 | E | Gateway — api_server, feishu_comment, wecom helpers | Platform coverage |
| 3 | D | Tools — CDP dead code, feature gaps | 300+ lines real code not wired |
| 4 | T | Tests — suite timeout, per-platform tests | Quality infra |
| 5 | J | Plugins — .so build, 6 plugin dirs | Plugin ecosystem |

### P2 — Feature Depth
| # | Sector | What |
|---|--------|------|
| 1 | I | TUI features — response wrapping, config editor, theme |
| 2 | M | Library test coverage |
| 3 | U | CI/CD cross-compile, release automation |

## Key Files
- **Battleship v4:** `.hermes/mind-palace/plans/battleship-v4.md` (~270 gaps)
- **DA v15:** `.hermes/mind-palace/da-audit-v15.md` (fresh survey)
- **State:** `.hermes/mind-palace/state.md`
- **Goal-Mantra:** `.hermes/mind-palace/goal-mantra.md`
- **Achievements:** `.hermes/vault/achievements.md`
- **Old battleship (archived):** `.hermes/vault/legacy-plans-archive.md`
