# Plan — Next Phase (v428)

S0: D09+D19+D20+D21 all PORTED. **C11 PORTED** (xAI OAuth callback login). 63 gaps. Suite 327/0/13.

**Latest:** Phase 372 — xAI OAuth loopback callback server. `/auth login xai-oauth` starts local HTTP server, displays PKCE authorize URL, waits for browser callback, exchanges code for tokens, saves to auth store. C11 depth improved from PARTIAL to PORTED.

**Next gap target:**
- S4 TUI backend (T01-T08: JSON-RPC, transport, render, WebSocket, entry, slash worker, event publisher, app layout)
- S7 Test coverage

**Structural gaps remaining by sector:**
- S4 TUI: 16 gaps (P1: T01-T08; P2-P3: T19-T28)
- S5 CLI: 10 gaps (C19-C30 REAL GAP)
- S7 Tests: 19 clusters (P1)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
