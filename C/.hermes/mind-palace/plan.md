# Hermes C Translation — Plan (v29 — 2026-05-25)

## Mission

P0: WuBu Slermes — C Translation. 1:1 parity with Python Hermes.
Every Python library, provider adapter, tool function, config key → C structs, function pointers, switch statements.
182 gaps is a checklist. Every ✅ without runtime verification is a lie.

## Verified State

| Metric | Value | Source |
|--------|-------|--------|
| Suite | 241/0/0 (203 files) | `test_runner.sh` |
| Binary | 29MB, 0 errors, 0 warnings | `make` |
| Source files | 153 .c, 66 .h | `ls` |
| Libraries | 58 | `lib/lib*` |
| Tools | 84 registered | `registry_register` grep |
| CLI | 79 commands | `commands.c` grep |
| Providers | 11 modules, all tested | `ls src/agent/provider_*.c` + tests |
| Gateways | 19 platforms | `ls src/gateway/platforms/` |
| Plugins | 10 C plugins | `ls src/plugins/` |
| Stubs | 0 real | all resolved |
| Real gaps | 182 (22 sectors, 0 P1) | battleship-v8, resolved items stripped |
| Python modules scanned | 77 agent, 88+ tools, 31 gateways | Upstream check |

## Milestones

| # | Milestone | Done | Remaining |
|---|-----------|------|-----------|
| 1 | All stubs resolved (P1-P3, 9 items) | 0/9 | ~300 LOC |
| 2 | All dead code wired/removed (15 items) | 0/15 | ~400 LOC |
| 3 | Missing agent modules ported (17 items) | 0/17 | ~5000 LOC |
| 4 | Agent module depth gaps closed (15 items) | 0/15 | ~3000 LOC |
| 5 | Subdirectory modules ported (22 items) | 0/22 | ~4000 LOC |
| 6 | Tool depth gaps closed (18 items) | 0/18 | ~2500 LOC |
| 7 | Gateway depth gaps closed (25 items) | 0/25 | ~7000 LOC |
| 8 | All config keys read (19 items) | 1/19 | ~450 LOC |
| 9 | Library depth features added (28 items) | 0/28 | ~2500 LOC |
| 10 | Bug fixes (15 items) | 0/15 | ~500 LOC |
| 11 | Test coverage (25 items) | 0/25 | ~5000 LOC* |
| 12 | Full parity passes suite | 0/1 | growth target |

*Test files are counted in LOC but are lower cognitive cost — one session per file.

## Next Actions (top 8)

1. □ Start A02 — context_compressor.py port (largest gap, ~1748 LOC)
2. □ Start A03 — conversation_compression.py port (603 LOC)
3. □ Add D02 — skill breakdown to insights tool (80 LOC)
4. □ Port A23 — nous_rate_guard.py (325 LOC)
5. □ Port A27 — rate_limit_tracker.py (246 LOC)
6. □ Add D16 — plugin memory provider interface (280 LOC)
7. □ G01 — Home Assistant conversation loop (200 LOC)
8. ✅ G04 — DingTalk inbound polling (80 LOC) — complete
9. ✅ G05 — WeCom inbound polling (80 LOC) — complete
10. ✅ G06 — SMS webhook wiring (50 LOC) — complete
11. ✅ G07 — Mattermost bot-message filtering (30 LOC) — complete
12. ✅ G02 — QQ Bot inbound polling (150 LOC) — complete
11. ✅ G07 — Mattermost bot-message filtering (30 LOC) — complete

## Recently Resolved

| ID | Description | Date |
|----|-------------|------|
| P01 | Anthropic ephemeral cache headers | 2026-05-24 |
| D04 | Insights empty-state handling | 2026-05-24 |
| B10 | Process health check action | 2026-05-24 |
| B03 | WSL path translation | 2026-05-24 |
| R04 | HomeAssistant input_text reset after poll | 2026-05-24 |
| C03 | agent.model_metadata config key | 2026-05-24 |
| S05 | /curator run — stale claim retired | 2026-05-24 |
| B05,B06 | Gateway crash + db leak — stale claims retired | 2026-05-24 |
| C01-C05,C16 | 6 config keys — stale claims retired | 2026-05-24 |
| D04,D17-D20 | Insights + file backend depth — resolved | 2026-05-24 |
| R04/W10 | HomeAssistant poll reset | 2026-05-24 |
| P01 (S16) | Anthropic ephemeral cache headers | 2026-05-24 |
