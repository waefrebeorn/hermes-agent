# Slermes C — Roadmap (May 24 DA-v1)

## Status: ✅ P0 Complete — 1:1 Core Parity

All critical subsystems have been translated to C. What remains is polish.

## Completed Phases (1-170)

| Block | Phases | Area | Status |
|-------|--------|------|--------|
| Foundation | 1-10 | Libraries (json, http, yaml, crypto, cron, proc, tui, db, plugin, skin) | ✅ |
| Agent core | 11-20 | Loop, LLM client, context, CLI, provider, config | ✅ |
| Tools core | 21-40 | 27 tool handlers (48+ registrations) | ✅ |
| Gateway core | 41-50 | 20 platform adapters + server | ✅ |
| Security | 51-60 | URL safety, path traversal, Tirith, approval | ✅ |
| Providers | 61-70 | OpenAI, Anthropic, Google | ✅ |
| TUI | 71-80 | ncurses split-screen + agent dispatch + streaming | ✅ |
| Plugin | 81-90 | dlopen discovery + init + cleanup | ✅ |
| CLI parity | 91-100 | 69 commands, aliases, help, dispatch | ✅ |
| Testing | 101-110 | 43 tests, parity checker | ✅ |

## Remaining Polish (P2-P3)

| Phase | Area | What | Priority |
|-------|------|------|----------|
| 111 | Session DB | FTS5-style search (post-MVP) | P3 |
| 112 | Config | Full key coverage (personality, verbose, yolo, fast) | P2 |
| 113 | Agent loop | Retry/failover logic | P2 |
| 114 | Agent loop | Smart context compression | P3 |
| 115 | Tools | Full CDP browser integration (requires server) | P2 |
| 116 | Security | Persistent allowlist | P2 |
| 117 | Cross-platform | macOS computer_use support | P2 |

## Never (Platform-Locked)

| Feature | Reason |
|---------|--------|
| computer_use on Linux | Requires macOS cua-driver |
| CDP browser_vision | Requires external CDP server |
