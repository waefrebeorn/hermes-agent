# State — Hermes C Translation (2026-05-29, Session 8)

**~43.2% parity — 216 of 500 gaps closed (battleship v3).**

## Dashboard
| Category | Done | % | Notes |
|----------|------|---|-------|
| Core | 12/16 | 75% | Solid |
| Agent | 38/115 | 33% | rate_limit_tracker.py ported |
| CLI | 14/95 | 15% | /secrets command added |
| Tools | 41/92 | 45% | 68 registered + test_runner fixes |
| Gateway | 22/64 | 34% | 19 platforms |
| MCP | 2/11 | 18% | stdio + server done |
| ACP | 1/9 | 11% | Basic server |
| Cron | 3/3 | 100% | Done |
| TUI | 1/8 | 12% | Experimental |
| Plugins | 10/26 | 38% | 10 .so, 16 to port |
| Config | 4/6 | 67% | 322 keys |
| Build/Doc | 9/11 | 82% | Docker fixed |
| Security | 7/10 | 70% | Sandbox, URL safety, file_safety |
| Provider | 11/18 | 61% | 9 native + metadata |
| Stubs | 4/10 | 40% | ALL stubs resolved |
| Tests | 10/12 | 83% | T01-T09 + ansi_strip + binary_extensions tests |
| CI/CD | 10/10 | 100% | All U gaps closed |
| **Total** | **216/500** | **43.2%** | **284 gaps remaining** |

## Session Log
- **Session 13 (May 29):** Updated shell completions (bash/zsh) with new subcommands. Added `hermes version` CLI subcommand. Updated help text. Suite: 195/0/0. Parity: 216/500.
- **Session 12 (May 29):** Added CLI subcommand dispatch — `hermes status|dump|logs|tools|plugins|secrets|skills|cron|help` now work as shell subcommands, not just slash commands. Also updated help text. Suite: 195/0/0. Parity: 216/500.
- **Session 8 (May 29):** test_runner.sh fix — 4 skipped tests (skill_manage, managed_gateway, rate_limit, skill_mgmt_tool) fixed. Root causes: missing include dirs + wrong .c file linked. Suite: 195/0/0 (0 skipped). Parity: 216/500.
- **DA v38 (May 24, Session 7):** managed_tool_gateway.py port — Nous vendor gateway helpers (12 assertions). Suite: binary builds clean. Parity: 214/500 (42.8%).

## Build Status
```
Suite:  195/0/0     (145 tests, ~1005 assertions)
Binary: 9.1MB       (dynamic, -O2 -g)
Errors: 0           (make -j$(nproc))
Warnings: ~40       (Wformat-truncation, -Wpedantic, unused params)
CI:     c-build.yml (Linux x86_64 + Docker)
```
