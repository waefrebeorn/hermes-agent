# Prestige (v356)

## P0 — Architecture (4 gaps)
F01: C can't hook Python
F02: Test count mismatch (289 vs ~1262)
F03: No Python interop
F09: No async event loop

## P1 — TUI (14), Test coverage (9), Provider adapters (2), Architecture (2), CLI (1) = 28

T01-T14: TUI backend + React frontend (28 total, 14 P1)
X01-X09: Test coverage (19 clusters, 1000+ tests)
F05: Architecture gaps (1 P1)
C11: Auth/OAuth system (1 P1, Phase 290: OAuth status in /secrets)

## Phase Map

| Phase | Focus | Sectors | Gaps |
|-------|-------|---------|------|
| Phase 0 | S0 D09 vi mode + S4 T01-T14 TUI backend | S0, S4 P1 | ~16 |
| Phase 4 | CLI ecosystem | S5 (20 gaps: C01 C03 C11 C13-C18 + 12 others) | ~20 |
| Phase 2 | Test coverage campaign | S7 | 19* (1000+ tests) |
| Phase 5 | Plugin system + Architecture gaps | S9, S10 | ~27 |

## Strategy

S0+S1+S3+S6+S8+R02+R04+R10 PORTED. S5 25→20 (C04+C05 stale retired, C10+C12 WON'T PORT).
