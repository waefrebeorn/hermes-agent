# Slermes C — State Dashboard (v49 — 2026-05-27)

## Build Metrics
Build clean. **83 unique tools** (registry_register). 98 CLI commands (COMMANDS[] table). 19 gateways. 10 provider types + metadata utility. 59 libs. 146 src/ .c files (non-deps). 224 test_*.c files. Binary: 30M. Suite: 257/0/0.

## 1:1 Parity Status (Triple DA v16)
~314 item-level gaps (battleship-v16 rows, 26 stale 1B claims removed).

## Recent (this session)
- file_batch: added batch rename action. POSIX glob-based pattern matching with wildcard extraction/substitution. Supports "pattern" + "dest_pattern" params (e.g. pattern="*.txt" + dest_pattern="backup_*.md"). Falls back to files+dest for simple rename. Closes 1 tool-depth gap (batch rename).
- file_batch: added hash action (SHA-256 via libhash). Reads file, returns hex sha256. 100MB cap. Closes 1 tool-depth gap (batch hash).
- terminal.c: fixed JSON schema bug - stray escaped quote after `modal` in backend description. Schema parser was lenient but would reject at strict parse. Removed extra quote between `}` and `,`.
- file_batch: added batch chmod action (`mode` param, octal e.g. '755'). Parse mode string -> mode_t, sandbox-checked per file, per-file result reporting. Closes 1 tool-depth gap for file_operations (batch chmod/chown).
- file_batch: added batch touch action. Creates file if missing, updates timestamp if exists. Uses utimensat() + fopen() fallback. Closes 1 tool-depth gap (batch touch).

## Phase Order
0. Display Parity (16) — 14/16 done
1. CLI Args — ALL DONE
2. Provider Parity (~20 real)
3. Tool Features (15)
4. Missing Tools (46)
5-11: Gateway (51) → Agent (74) → Plugins (13) → Libs (19) → Security (15) → Tests (50) → Infra (10)
