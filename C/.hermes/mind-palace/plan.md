# Plan (v116)

## Phase: REAL PARITY + UPSTREAM SYNC
33 gaps across 5 sectors. S4 (Upstream Drift) is NEW — 7583 upstream commits unported.

## Sector Breakdown

### S0: Form-vs-Function (P0) — 5 gaps
- F01: Retry broken in TUI — fix agent retry in terminal context
- F02: Setup not 1:1 — add interactive setup flow parity
- F03: Terminal resize race — fix SIGWINCH handling
- F04: C can't hook Python — create Python bridge module
- F05: Test cheating — port real behavioral tests (not just compilation tests)

### S1: Pipeline & Integration (P1) — 5 gaps
- P01: Retry broken in TUI (agent side)
- P02: Plumbing edge cases
- P03: Linkage/dependency integrity
- P04: TUI display bugs
- P05: General usage bugs

### S2: Cross-Comparison (P1) — 4 gaps
- A01: Full AST tool comparison (every C tool vs Python equivalent)
- A02: Test suite recreation (17k Python tests → C equivalents)
- A03: Behavioral parity (same input → same output)
- A04: JSON schema parity (tool schemas match Python exactly)

### S3: Product Features (P2) — 6 gaps
- Q01-Q06: Multi-turn, session persistence, plugins, skin, gateway, provider parity

### S4: Upstream Drift (P1) — 7 gaps (NEW)
- U01: Provider/API changes — auth flows, response formats, retry logic
- U02: Tool schema drift — Python schemas changed since C ported
- U03: Agent loop changes — retry, interrupt, budget, streaming
- U04: Gateway platform updates — new platforms, updated APIs
- U05: MCP updates — TLS mTLS, catalog, SSE improvements
- U06: Security/auth overhaul — dashboard-auth, OAuth PKCE, credential pool
- U07: Test suite growth — ~17k tests vs 239 C tests

## Immediate Next Step
Audit S4 upstream commits → categorize by impact → port critical fixes first.
Then resume S0-S3 gap closure per battleship v27 priority.
