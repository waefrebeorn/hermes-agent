# Hermes C Translation — Plan (v30 — 2026-05-25)

## Mission

P0: WuBu Slermes — C Translation. 1:1 parity with Python Hermes.
Every Python library, provider adapter, tool function, config key → C structs, function pointers, switch statements.
147 gaps is a checklist. Every ✅ without runtime verification is a lie.

## Verified State

| Metric | Value | Source |
|--------|-------|--------|
| Suite | 224/0/21 (207 files) | `test_runner.sh` |
| Binary | 30MB, 0 errors, 0 warnings | `make` |
| Source files | 154 .c, 66 .h | `ls` |
| Libraries | 59 | `lib/lib*` |
| Tools | 84 registered | `registry_register` grep |
| CLI | 79 commands | `commands.c` grep |
| Providers | 11 modules, all tested | `ls src/agent/provider_*.c` + tests |
| Gateways | 19 platforms | `ls src/gateway/platforms/` |
| Plugins | 10 C plugins | `ls src/plugins/` |
| Stubs | 0 real | all resolved |
|| Real gaps | 147 (22 sectors, 0 P1) | battleship-v8 |
| API server | 1015 LOC, 12 endpoints | `wc -l src/api_server.c` |

## Next Actions (top 6)

1. □ D22 — Feishu doc/drive tool support (150 LOC, S7)
2. □ N02 — Mixture of Agents tool (300 LOC, S20)
3. □ U01 — TUI image display — code exists but unwired (150 LOC, S14)
4. □ U02 — TUI session browser with search (200 LOC, S14)
5. □ T04-T25 — Test coverage for remaining untested modules (S12)
6. □ U04 — TUI config editor (150 LOC, S14)
