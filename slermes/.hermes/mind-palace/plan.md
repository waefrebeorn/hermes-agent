# Plan — Next Phase (v432)

S0: D09+D19+D20+D21 all PORTED. C11 PORTED. **T07+T06+T05+T01 PORTED**.
59 gaps. Suite 331/0/13.

**Latest:** Phase 376 — TUI JSON-RPC Gateway Server (T01). Method
dispatch table with 2 built-in methods (ping, echo) + custom method
registration. JSON-RPC 2.0 request/response handling with standard error
codes. Parameter extraction API (string/int/bool/double). Replaces
ad-hoc strstr/sscanf inline parsing. 13-test suite. S4 depth improved
from 13→12 gaps.

**Next gap target:**
- S4 TUI backend (T02-T04, T08)
- S7 Test coverage

**Structural gaps remaining by sector:**
- S4 TUI: 14 gaps (P1: T02-T04, T08; P2-P3: T19-T28)
- S5 CLI: 10 gaps (C19-C30 REAL GAP)
- S7 Tests: 19 clusters (P1)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
