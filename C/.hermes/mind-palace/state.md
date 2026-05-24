# State — Hermes C Translation (2026-05-24, DA v15 — Battleship Reset)

**~60% estimated parity — ~270 gaps remaining (fresh count).**

## Dashboard

| Category | C | Python | % | Notes |
|----------|---|--------|---|-------|
| Agent | 44 .c files | 77 .py files | 57% | 33 of 77 ported |
| CLI | 8 .c files | 88 .py files | **9%** | 40 real cmd_, 197 stubs |
| Tools | 31 init funcs, ~83 reg | 75 .py files, ~68 reg | — | C registers more tool variants |
| Gateway | 19 platform .c | 31 platform modules | 61% | Missing api_server + 12 helpers |
| MCP | 1 .c + lib | 1 module | ~90% | Sampling, transports done |
| ACP | 5 .c files | 9 .py files | 56% | Server, events, permissions done |
| Cron | 3 .c files | 3 modules | **100%** | ✅ Complete |
| TUI | 2 .c + lib | 1 Ink app | ~25% | Session browser done |
| Plugins | 10 .c src files | 16 plugin dirs | 63% | 0 .so built |
| Provider | 9 native C | 28 plugin dirs | 32% | Big gap |
| Config | ~322 keys | 432 nested keys | ~75% | Needs exact audit |
| Security | 7 modules | — | ~70% | url_safety, file_safety, rate_limit done |
| Stubs | 1 true stub | — | **99% clean** | browser_cdp handler not wired to real CDP code |
| Tests | 173 .c files | ~28K py tests | — | Timeout at 120s |

## Build Status

Suite:  ~213/0/0  (173 test files — cannot complete in 120s)
Binary: 29MB     (dynamic ELF, -O2 -g)
Errors: 0        (make -j$(nproc))
Warnings: 0

## Current Reality

- C source: 44 agent .c, 31 tool .c, 19 gateway .c, 5 acp .c, 8 cli .c, 10 plugin .c
- Library dirs: 57 lib/lib*/
- Test files: 173 test_*.c
- Tool registrations: ~83 (31 init functions called from tool_init.c)
- Gateways: 19 platform files
- Providers: 9 native (openai, deepseek, openrouter, xai, anthropic, google, azure, bedrock, custom)
- CLI: 237 cmd_ functions declared — ~40 real, ~197 stubs
- Plugins: 10 .c source files (honcho, kanban, spotify, disk_cleanup, file_memory, achievements, observability, skills, image_gen, google_meet) — 0 .so on disk
- Git: 740+ commits on main
- LLM: Working — DeepSeek v4 Flash via OpenAI-compat
- CDP browser: 300+ lines real client code exists but tool registered to stub handler (dead code)

## Key Gaps (from battleship-v4)

| Sector | Gap Count | Top Item |
|--------|-----------|----------|
| CLI/C | ~80 | 197 stub commands, 80 unported modules |
| Agent/B | ~44 | 35+ unported Python modules |
| Providers/K | ~19 | 19 missing provider plugins |
| Gateway/E | ~12 | api_server, feishu_comment, wecom helpers |
| Tests/T | ~20 | Suite timeout, per-platform tests |
| CLI Depth/Q | ~10 | Readline, autocomplete, Rich output |
