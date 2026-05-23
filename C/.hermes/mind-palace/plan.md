# Plan — Hermes C Translation (v11 — 2026-05-23)

## Phase Completion
| Phase | Description | Status |
|-------|-------------|--------|
| P1 | Foundation deps (JSON/YAML/HTTP/DB/crypto) | 100% |
| P2 | Agent core (loop, LLM client, CLI, config) | 100% |
| P3 | Tools (registry, 68 handlers) | 35% |
| P4 | Gateway (19 platform adapters) | 34% |
| P5 | Cron (scheduler, SQLite, retry, templates) | 100% |
| P6 | Plugins (10 .so) | 38% |
| P7 | Libraries (30 units) | 100% |
| P8 | Tests (117 files, 154/0/0) | 66% |
| P9 | Stub remediation | 0% |
| PA | CI/CD infrastructure | 0% |

## Next Steps

### P0 Next Session
1. **S01-S03**: computer_use real backend — MCP client for cua-driver
2. **U01-U02**: CI must pass gate + Docker build verification
3. **S04-S06**: browser CDP WebSocket client for Chrome/Playwright

### P1 Queue
4. **T01-T02**: Gateway platform tests + CLI depth coverage
5. **S07**: image_gen Fal AI REST client
6. **S10**: TUI session DB-backed browser
7. **D75-D79**: computer_use upstream Python backports

### P2 Backlog
8. **U04**: ASan CI job
9. **C91-C95**: CLI autocomplete, rich formatting, secrets CLI
10. **T08-T09**: Fuzz testing + memory leak detection

## Known Bugs
See `vault/bug-bounty.md` (16 total, 6 critical, 10 high).
