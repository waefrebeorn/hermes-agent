# Prestige (v327)

## P0 — Architecture (4 gaps)
F01: C can't hook Python
F02: Test count mismatch (292 vs ~1262)
F03: No Python interop
F09: No async event loop

## P1 — TUI (14), Test coverage (9), Provider adapters (2), Architecture (2), CLI (1) = 28

T01-T14: TUI backend + React frontend (28 total, 14 P1)
X01-X09: Test coverage (19 clusters, 1000+ tests)
R02-R04: Provider adapters (3 real gaps, P1)
F05, F07: Architecture gaps (2 P1)
C11: Auth/OAuth system (1 P1)

## Phase Map

| Phase | Focus | Sectors | Gaps |
|-------|-------|---------|------|
| Phase 0 | S0 D09 vi mode + S4 T01-T14 TUI backend | S0, S4 P1 | ~16 |
| Phase 1 | Provider adapters | S8 (R02,R04) | ~2 |
| Phase 2 | Test coverage campaign | S7 | 19* (1000+ tests) |
| Phase 4 | CLI ecosystem | S5 | ~30 |
| Phase 5 | Plugin system + Architecture gaps | S9, S10 | ~27 |

## Strategy

S0+S1+S3+S6+S8(R05-R09 WON'T PORT) all PORTED. Focus shifts to S0 D09 vi depth,
S8 R02/R03/R04 provider adapters, and S7 test expansion.
