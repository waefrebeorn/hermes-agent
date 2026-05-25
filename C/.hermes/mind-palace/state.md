# Hermes C — State Dashboard (v48 — 2026-05-25)

## Build Metrics (code-verified)

| Metric | Value | As Of |
|--------|-------|-------|
| Suite | **244/0/0** — 206 test files | 2026-05-25 |
| Binary | **29MB ELF**, 0 warnings | 2026-05-25 |
| Source `.c` files | **153** | 2026-05-25 |
| Headers | **66** | 2026-05-25 |
| Library directories | **58** | 2026-05-25 |
| Tools registered | **84** (all real handlers) | 2026-05-25 |
| CLI commands | **79** | 2026-05-25 |
| Gateway platforms | **19** | 2026-05-25 |
| Agent `.c` modules | **51** | 2026-05-25 |
| Provider modules | **11** (all with tests) | 2026-05-25 |
| Provider test files | **11** | 2026-05-25 |
| C plugins | **10** | 2026-05-25 |
| Git commits | **873+** | 2026-05-25 |

## Battleship Status

- **Active:** battleship-v8 (176 verified gaps across 22 sectors)
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
| S7 | Tool Depth | 2 | 0 | 1 | 1 |
| S8 | Gateway Depth | 17 | 0 | 15 | 2 |
| S9 | Config/Environment | 0 | 0 | 0 | 0 |
| S10 | Library Depth | 11 | 0 | 5 | 6 |
| S11 | Bug Fixes | 2 | 0 | 1 | 1 |
| S12 | Test Coverage | 20 | 0 | 20 | 0 |
| S13 | API Server | 5 | 0 | 5 | 0 |
| S14 | TUI Depth | 8 | 0 | 7 | 1 |
| S15 | Curator | 0 | 0 | 0 | 0 |
| S16 | Prompt Caching | 3 | 0 | 2 | 1 |
| S17 | Shell Hooks | 3 | 0 | 3 | 0 |
| S18 | Vault Encryption | 3 | 0 | 2 | 1 |
| S19 | Security | 5 | 0 | 3 | 2 |
| S20 | New Features | 9 | 0 | 2 | 7 |
| S21 | Refactoring | 9 | 0 | 2 | 7 |
| S22 | CI/Integration | 7 | 0 | 6 | 1 |
| **Total** | **176** | **0** | **101** | **75** |

## Python Upstream Parity

| Category | Python | C | Coverage |
|----------|--------|---|----------|
| Agent modules | 77 | 50 (26 direct match + 24 C-only) | ~65% coverage |
| Tool files | 88+ | 43 tool `.c` files | ~49% files |
| Gateway platforms | 31 | 19 | 61% |
| Plugins | 138 dirs (many optional-shared) | 10 C plugins | core plugins done |

## Stale Claims Retired This Session

- **N01**: Bitwarden Secrets Manager — secrets.c (330 LOC) + hermes_secrets.h (55 LOC) already implement full integration

- **D23**: Web search provider abstraction — web_search_registry.c (239 LOC + 217 test) already exists

## Recently Resolved

| ID | Description | Sector | Date |
|----|-------------|--------|------|
| T24 | voice_mode.c — 20 tests (state mgmt, config, args) | S12 | 2026-05-25 |
| D14 | Browser supervisor — cdp_supervisor_ping() with Browser.getVersion | S7 | 2026-05-25 |
| D15 | Camofox session persistence — save/load/delete browser state | S7 | 2026-05-25 |
| D10 | Computer use backend registry — register/list/clear backends, CU_BACKEND env | S7 | 2026-05-25 |
| D11 | Vision routing — vision→som fallback with notification | S7 | 2026-05-25 |
| C08 | Config key agent.codex_runtime (auto\|codex_app_server) | S9 | 2026-05-25 |
| C11 | MoA config keys (enabled, model, strategy, workers) | S9 | 2026-05-25 |
| D07 | Modal terminal backend — run_command_modal() via `modal run` | S7 | 2026-05-25 |
| D08 | file_sync library — collect mkdir upload_all (14 tests) | S7 | 2026-05-25 |
| T11 | image_gen.c — 8 tests (null args, error paths, validation) | S12 | 2026-05-25 |
| T03 | clarify.c — 8 tests (null args, schema, error paths) | S12 | 2026-05-25 |
| C17 | agent.checkpoint.* — already implemented (8 fields, YAML reader) | S9 | 2026-05-25 |
| I01 | GitHub Actions CI — already exists (.github/workflows) | S22 | 2026-05-25 |
| G22 | Missing gateway platforms — both Python and C have ~19 (partially stale) | S8 | 2026-05-25 |
| D23 | Web search provider abstraction — web_search_registry.c exists | S7 | 2026-05-25 |

## Next Priority Queue (top 10)

| Rank | ID | Description | LOC | Sector |
|------|----|-------------|-----|--------|
| 1 | E01 | API server health endpoint + REST endpoints | 1500 | S13 |
| 2 | E02 | OpenAI-compatible /v1/chat/completions proxy | 500 | S13 |
| 3 | D22 | Feishu doc/drive tool support | 150 | S7 |
| 4 | N01 | Bitwarden Secrets Manager integration | 200 | S20 |
| 5 | D22 | Feishu doc/drive tool support | 150 | S7 |
| 6 | N02 | Mixture of Agents tool | 300 | S20 |
| 7 | U01 | TUI image display (sixel/kitty/iterm2) — code exists but unwired | 150 | S14 |
| 8 | U02 | TUI session browser with search | 200 | S14 |
| 9 | T04-T25 | Test coverage for 23 remaining untested modules | — | S12 |
| 10 | N03 | Feishu doc and drive tools | 250 | S20 |
