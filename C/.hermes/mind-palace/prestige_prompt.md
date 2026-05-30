# Prestige (v279)

## P0 — Architecture (4 gaps)
F01: C can't hook Python
F02: Test count mismatch (279 vs ~1262)
F03: No Python interop
F09: No async event loop

## P1 — TUI (14), Test coverage (9), Provider adapters (6), Architecture (3), CLI (1), Plugin (1) = 34

T01-T14: TUI backend + React frontend (28 total, 14 P1)
X01-X09: Test coverage (19 clusters, 1000+ tests)
R01-R06: Provider adapters (10 total, 6 P1)
F05, F08, F10: Architecture gaps (3 P1)
C11: Auth/OAuth system (1 P1)
P01: Plugin loading/lifecycle (1 P1)

## Phase Map

| Phase | Focus | Sectors | Gaps |
|-------|-------|---------|------|
| Phase 1 | Agent plumbing + Provider adapters + TUI backend | S1 (5), S8 (6), S4 P1 (14) | ~25 |
| Phase 2 | Test coverage campaign | S7 | 19* (1000+ tests) |
| Phase 4 | CLI ecosystem | S5 | ~30 |
| Phase 5 | Plugin system + Architecture gaps | S9, S10 | ~30 |
| Phase 6 | Agent module depth | S2 (15 won't-port) + S8 remaining | ~12 |

## Strategy

S0+S3+S6 all PORTED. Focus shifts to S1 partials depth (L25-L28),
S7 test expansion, and B08 send_message remaining features.
