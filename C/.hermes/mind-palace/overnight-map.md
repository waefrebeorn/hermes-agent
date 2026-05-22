# Slermes C — Overnight Map (May 22 PM v3, 430-gap scope)

## Quick Trunk Reference
- Source: `/home/wubu/hermes-agent-dev/C/`
- Python ref: `/home/wubu/hermes-agent-dev/` (upstream)
- Config: `~/.slermes/config.yaml` + `.env`
- Build: `make -C . -j$(nproc)` | Tests: `bash test_runner.sh`
- Binary: `./hermes` (~3.4MB static ELF)
- Git: `wubu=git@github.com:waefrebeorn/hermes-agent.git`
- Roadmap: `.hermes/mind-palace/plans/400-gap-mega-roadmap.md`

## Where We Are
DA v6, session 11. 430-gap scope. ~55% complete.

## Biggest Findings
1. **7 C stubs** found via systematic grep: CDP browser tools(4), computer_use, memory_sqlite, memory_plugin. All return error/hardcoded data.
2. **Telegram 11x gap**: Python 5,465L vs C 479L. 16 send methods, 10 message types missing.
3. **LLM params**: ✅ G330-G333 (temperature, top_p, stop, max_tokens) wired through all 9 providers. G340 (service_tier) wired for OpenAI-compat. 6 params still missing.
4. **Tools shallow**: kanban(9 vs 25), browser(18 vs 158), memory(3 vs 22).
5. **14 Python libs** without C ports.
6. **10 session state fields** for tracking.

## Workstreams (pick one)
**A — Fix stubs [P0]:** CDP browser tools actually work for server-connected mode.
  computer_use stub. memory backends. vision LLM integration.
**B — Telegram depth [P1]:** Add send_voice, send_video, sticker/photo handling.
**C — LLM params [P1]:** Wire temperature/top_p/stop/seed/response_format through
  the provider interface.
**D — Tool depth [P2]:** Expand kanban(9→25), browser(18→158), memory(3→22).
**E — Tests [P2]:** G167 MCP test suite.

## Data Not To Re-Derive
- 430 total gaps now tracked. ~55% complete.
- 7 stubs + 3 shallow tools + 11 LLM params + 16 Telegram sends + 10 msg types
- All prior data (config 99%, providers 90%, tests 1422 assertions) still valid
