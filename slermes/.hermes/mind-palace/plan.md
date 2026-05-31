# Plan — Next Phase (v436)

S0-S3+S6+S8 all PORTED. S4: all P1 gaps resolved.
**T07+T06+T05+T01+T02+T08+T03+T04 PORTED.**
55 gaps. Suite 335/0/13.

**Latest:** Phase 380 — TUI WebSocket Support (T04). WebSocket server
wrapper for TUI with server lifecycle and client management. libwebsocket
extended: server API (ws_server_listen/accept/close/port), ws:// plain
connections, write_raw/read_raw helpers for SSL/plain transport.
4-test suite. S4 improved from 9→8 gaps.

**Next gap target:**
- S4 T19-T28 React tsx parity (P2-P3)
- S7 Test coverage (P1)
- S5 CLI ecosystem (P2)

**Structural gaps remaining by sector:**
- S4 TUI: 8 gaps (P2-P3: T19-T28)
- S5 CLI: 10 gaps (C19-C30 REAL GAP)
- S7 Tests: 19 clusters (P1)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
