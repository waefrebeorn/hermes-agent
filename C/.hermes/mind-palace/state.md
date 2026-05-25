# Hermes C — State Dashboard (v40 — 2026-05-24)

## Build Metrics (code-verified)

| Metric | Value | As Of |
|--------|-------|-------|
| Suite | **239/0/0** — 202 test files | 2026-05-24 |
| Binary | **29MB ELF**, 0 warnings | 2026-05-24 |
| Source `.c` files | **153** | 2026-05-24 |
| Headers | **66** | 2026-05-24 |
| Library directories | **58** | 2026-05-24 |
| Tools registered | **83** (all real handlers) | 2026-05-24 |
| CLI commands | **79** | 2026-05-24 |
| Gateway platforms | **19** | 2026-05-24 |
| Agent `.c` modules | **51** | 2026-05-24 |
| Provider modules | **11** (all with tests) | 2026-05-24 |
| Provider test files | **11** | 2026-05-24 |
| C plugins | **10** | 2026-05-24 |
| Git commits | **857+** | 2026-05-24 |

## Battleship Status

- **Active:** battleship-v8 (201 verified gaps across 22 sectors)
- **Retired:** battleship-v7 (all stale claims moved to vault)
- **Vault:** achievements.md updated with all completed work and retired stale claims

## Gap Summary

| Category | Gaps | P1 | P2 | P3 |
|----------|------|-----|-----|------|
| Confirmed Stubs | 0 | 0 | 0 | 0 |
| Placeholder/Unwired | 11 | 0 | 1 | 10 |
| Dead Code | 12 | 0 | 2 | 10 |
| Missing Agent Modules | 12 | 0 | 8 | 4 |
| Agent Module Depth | 15 | 0 | 12 | 3 |
| Missing Subdirectory Modules | 22 | 0 | 12 | 10 |
| Tool Depth Gaps | 10 | 0 | 8 | 2 |
| Gateway Platform Depth | 18 | 0 | 15 | 3 |
| Configuration | **3** | 0 | **0** | 3 |
| Library Depth | 26 | 0 | 18 | 8 |
| Bug Fixes | 2 | 0 | 1 | 1 |
| Test Coverage | 25 | 0 | 25 | 0 |
| API Server Depth | 5 | 0 | 5 | 0 |
| TUI Depth | 8 | 0 | 7 | 1 |
| Curator Depth | 3 | 0 | 2 | 1 |
| Prompt Caching | 5 | 0 | 4 | 1 |
| Shell Hooks | 3 | 0 | 3 | 0 |
| Vault Encryption | 3 | 0 | 2 | 1 |
| Security Depth | 6 | 0 | 4 | 2 |
| C-Only New Features | 10 | 0 | 4 | 6 |
| Refactoring | 10 | 0 | 2 | 8 |
| Integration & CI | 7 | 0 | 6 | 1 |
| **Total** | **216** | **0** | **140** | **76** |

## Python Upstream Parity

| Category | Python | C | Coverage |
|----------|--------|---|----------|
| Agent modules | 77 | 50 (26 direct match + 24 C-only) | ~65% coverage |
| Tool files | 88+ | 43 tool `.c` files | ~49% files |
| Gateway platforms | 31 | 19 | 61% |
| Plugins | 138 dirs (many optional-shared) | 10 C plugins | core plugins done |

## Known Stale Claims Retired

16 items from battleship-v7 proven stale — see vault/achievements.md Phase 8 for full table.
Key retired items: D09 (CUA existed), D12 (CDP existed), S01/S02 (browser stubs existed), S03/S04 (set_value fixed), B01 (buffer overflow fixed), L15-L17 (message queries added).

## Recently Resolved

| ID | Description | Sector | This Session |
|----|-------------|--------|-------------|
| P01 | Anthropic ephemeral cache headers — anthropic-beta header added | S16 | 2026-05-24 |
| D04 | Insights empty-state handling — message when no sessions | S7 | 2026-05-24 |
| B10 | Process health check action — subsystem status summary | S11 | 2026-05-24 |
| B03 | WSL path translation — Windows paths→/mnt/ in file tool | S11 | 2026-05-24 |
| R04 | HomeAssistant input_text reset after poll — was TODO comment | S2 | 2026-05-24 |
| C03 | agent.model_metadata config key — file path for custom model DB | S9 | 2026-05-24 |
| S05 | /curator run — stale claim, real llm_background_review existed | S1 | 2026-05-24 |
| B05 | Gateway crash callback — stale claim, null checks exist | S11 | 2026-05-24 |
| B06 | db_list_with_meta leak — stale claim, code is clean | S11 | 2026-05-24 |
| C01-C05,C16 | 6 config keys — stale claims, all had YAML readers | S9 | 2026-05-24 |
| D17-D20 | File backend depth — stale claims, all had real impls | S7 | 2026-05-24 |
| D04 | Insights empty-state handling | S7 | 2026-05-24 |
| R04/W10 | HomeAssistant poll reset | S21 | 2026-05-24 |
| P01 (S16) | Anthropic ephemeral cache headers | S16 | 2026-05-24 |
| A23 | nous_rate_guard.py port — cross-session rate limit guard | S4 | 2026-05-24 |
| A27 | rate_limit_tracker — stale, already in lib/libratelimit | S4 | 2026-05-24 |
| A02 | context_compressor.py port — core compression pipeline complete (tool pruning, redaction, LLM summary gen, scaled budget, boundary alignment, anti-thrashing) | S4 | 2026-05-24 |
| A03 | conversation_compression.py — orchestration done via agent_loop + auxiliary_client | S4 | 2026-05-24 |
| S07, S08 | Plugin memory vtable import_json + export_json — now wired | S1 | 2026-05-24 |
| B14 | Plugin load errors propagated — discover logs each failure via plugin_error(); main.c startup prints count; memory.c fallback logs load error | S11 | 2026-05-24 |
| B11 | Gateway config validation — setup_email() validates IMAP/SMTP/sendmail config; startup logs platform count | S11 | 2026-05-24 |
| B12 | Cron session context — HERMES_CRON_NOTIFY_CHANNEL env var wires notification delivery channel | S11 | 2026-05-24 |
| B15 | Gateway log rotation — gw_log_open writes to ~/.slermes/logs/gateway.log with 10MB rotation | S11 | 2026-05-24 |
| L08 | JSON pointer queries — json_pointer_get() added to libjson | S10 | 2026-05-24 |
| D13 | Stale retired — browser dialog handler already implemented | S7 | 2026-05-24 |

## Next Priority Queue (top 15)

| Rank | ID | Description | LOC | Sector |
|------|----|-------------|-----|--------|
| 1 | D16 | Plugin memory provider interface | 280 | S7 |
| 2 | G01 | Home Assistant conversation loop | 200 | S8 |
| 3 | G04 | DingTalk inbound polling | 80 | S8 |
| 4 | G05 | WeCom inbound polling | 80 | S8 |
| 5 | G06 | SMS inbound webhook wiring | 50 | S8 |
| 6 | T01-T25 | Test coverage for untested modules | — | S12 |
| 7 | C06 | gateway.secret_rotation | 30 | S9 |
| 8 | C10 | Skill auto-install config | 25 | S9 |
| 9 | C11 | Session auto-save compression level | 15 | S9 |
| 10 | F01 | File backend atomic writes | 30 | S11 |
