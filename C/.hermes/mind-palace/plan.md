# Hermes C Translation — Plan (v30 — 2026-05-25)

## Mission

P0: WuBu Slermes — C Translation. 1:1 parity with Python Hermes.
Every Python library, provider adapter, tool function, config key → C structs, function pointers, switch statements.
110 gaps is a checklist. Do not stop. Every ✅ without runtime verification is a lie.

## Verified State

| Metric | Value | Source |
|--------|-------|--------|
| Suite | 228/0/21 (207 test files) |
| Binary | 30MB, 0 errors, 0 warnings | `make` |
| Source files | 154 .c, 66 .h | `ls` |
| Libraries | 59 | `lib/lib*` |
| Tools | 84 registered | `registry_register` grep |
| CLI | 79 commands | `commands.c` grep |
| Providers | 11 modules, all tested | `ls src/agent/provider_*.c` + tests |
| Gateways | 19 platforms | `ls src/gateway/platforms/` |
| Plugins | 10 C plugins | `ls src/plugins/` |
| Stubs | 0 real | all resolved |
|| Real gaps | 145 (22 sectors, 0 P1) | battleship-v8 |
| API server | 1015 LOC, 12 endpoints | `wc -l src/api_server.c` |

## Next Actions (top 5)

1. □ U02 — TUI session browser with search (200 LOC, S14)
1. □ T04-T25 — Test coverage for remaining untested modules (S12)
2. □ U04 — TUI config editor (150 LOC, S14)
5. □ N03 — Feishu doc and drive tools (250 LOC, S20)
