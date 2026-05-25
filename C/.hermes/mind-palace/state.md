# Hermes C — State Dashboard (v41 — 2026-05-24)

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

- **Active:** battleship-v8 (193 verified gaps across 22 sectors)
- **Retired:** battleship-v7 (all stale claims moved to vault)
- **Vault:** achievements.md updated with all completed work and retired stale claims

## Gap Summary (from battleship-v8)

| Sector | Name | Gaps | P1 | P2 | P3 |
|--------|------|------|-----|-----|------|
| S1 | Stubs | 0 | 0 | 0 | 0 |
| S2 | Placeholder/Unwired | 11 | 0 | 1 | 10 |
| S3 | Dead Code | 12 | 0 | 2 | 10 |
| S4 | Missing Agent Modules | 12 | 0 | 8 | 4 |
| S5 | Agent Module Depth | 15 | 0 | 12 | 3 |
| S6 | Missing Subdirectory | 22 | 0 | 12 | 10 |
| S7 | Tool Depth | 8 | 0 | 6 | 2 |
| S8 | Gateway Depth | 17 | 0 | 15 | 2 |
| S9 | Config/Environment | 3 | 0 | 0 | 3 |
| S10 | Library Depth | 11 | 0 | 5 | 6 |
| S11 | Bug Fixes | 2 | 0 | 1 | 1 |
| S12 | Test Coverage | 25 | 0 | 25 | 0 |
| S13 | API Server | 5 | 0 | 5 | 0 |
| S14 | TUI Depth | 8 | 0 | 7 | 1 |
| S15 | Curator | 0 | 0 | 0 | 0 |
| S16 | Prompt Caching | 5 | 0 | 4 | 1 |
| S17 | Shell Hooks | 3 | 0 | 3 | 0 |
| S18 | Vault Encryption | 3 | 0 | 2 | 1 |
| S19 | Security | 5 | 0 | 3 | 2 |
| S20 | New Features | 10 | 0 | 2 | 8 |
| S21 | Refactoring | 9 | 0 | 2 | 7 |
| S22 | CI/Integration | 7 | 0 | 6 | 1 |
| | **Total** | **193** | **0** | **119** | **74** |

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
| L11 | yaml_parse_multi() — multi-document YAML (--- separator) | S10 | 2026-05-24 |
| L31 | @every N[m\|h] duration syntax in cron | S10 | 2026-05-24 |
| R07 | make check target — combined lint + build + test | S21 | 2026-05-24 |
| L05 | HTTP cookie jar — Set-Cookie parse + Cookie header build | S10 | 2026-05-24 |
| D05 | Docker backend stale — run_command_docker() already exists | S7 | 2026-05-24 |
| D06 | SSH backend stale — run_command_ssh() already exists | S7 | 2026-05-24 |
| R02,R03,R05 | Curator stale — /curator with status/run all implemented | S15 | 2026-05-24 |
| F01 | File backend atomic writes — mkstemp+fsync+rename | S11 | 2026-05-24 |
| S02 | Port scan detection — nmap/masscan/nc/zmap/dev/tcp detection in tirith | S19 | 2026-05-24 |
| L19 | Session tags CRUD — db_tag_add/remove/list/find | S10 | 2026-05-24 |
| L06 | HTTP redirect following — 301/302/303/307/308, relative URL resolution | S10 | 2026-05-24 |

## Next Priority Queue (top 10)

| Rank | ID | Description | LOC | Sector |
|------|----|-------------|-----|--------|
| 1 | E01 | API server health endpoint + REST endpoints | 1500 | S13 |
| 2 | E02 | OpenAI-compatible /v1/chat/completions proxy | 500 | S13 |
| 3 | D07 | terminal.c — Modal/Daytona/singularity backends | 500 | S7 |
| 4 | G22 | Missing 10 gateway platforms from Python | 3000 | S8 |
| 5 | D14 | Browser supervisor | 200 | S7 |
| 6 | N01 | Bitwarden Secrets Manager integration | 200 | S20 |
| 7 | C08 | agent.codex_app_server config | 40 | S9 |
| 8 | C11 | agent.mixture_of_agents config | 30 | S9 |
| 9 | F01 | File backend atomic writes | 30 | S11 |
| 10 | T01-T25 | Test coverage for untested modules | — | S12 |
