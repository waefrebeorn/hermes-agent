# Prestige (v116)

## P0 — Form-vs-Function (5 gaps)
F01: Retry broken in TUI — agent fails on retry in terminal UI
F02: Setup not 1:1 — missing interactive setup wizard parity
F03: Terminal resize race — SIGWINCH handler unreliable
F04: C can't hook Python — standalone C, no Python module import bridge
F05: Test cheating — 239 C tests vs ~17k Python tests

## P1 — Pipeline & Cross-Compare + Upstream Drift (12 gaps)
P01-P05: Plumbing, TUI bugs, linkage integrity, display bugs, general bugs
A01-A04: Full AST tool comparison, test recreation, behavioral parity, schema parity
U01-U07: Provider/API shifts, tool schema drift, agent loop changes, gateway changes, MCP updates, security/auth overhaul, test suite gap

## P2 — Product Features (6 gaps)
Q01-Q06: Multi-turn conversation, session persistence, plugin system, skin engine parity, gateway parity, provider mode parity

## Total: 33 gaps — P0:5, P1:12, P2:6
**Honest assessment**: Stub-hunt complete. Real parity phase with upstream drift adds 7 gaps in S4.
