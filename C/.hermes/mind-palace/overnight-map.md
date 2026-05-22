# Slermes C — Overnight Map (May 22 PM v4, 430-gap scope, ~57% complete)

## Quick Trunk Reference
- Source: `/home/wubu/hermes-agent-dev/C/`
- Python ref: `/home/wubu/hermes-agent-dev/` (upstream)
- Config: `~/.slermes/config.yaml` + `.env`
- Build: `make -C . -j$(nproc)` | Tests: `bash test_runner.sh` (58/59 pass)
- Binary: `./hermes` (~3.4MB static ELF)
- Git: `wubu=git@github.com:waefrebeorn/hermes-agent.git`
- Roadmap: `.hermes/mind-palace/plans/400-gap-mega-roadmap.md`

## Where We Are
DA v6, sessions 12+12b. 430-gap scope. ~57% complete. 11 LLM param gaps closed across 2 sessions.

## LLM Params Status
10/12 wired through all 9 providers (6 OpenAI-compat get all 10, 3 non-OpenAI get 4):
- ✅ max_tokens, temperature, top_p, stop_sequences (all 9)
- ✅ service_tier, presence_penalty, frequency_penalty, seed, logprobs/top_logprobs, user (6 OpenAI-compat)
- ⬜ response_format (G336) — needs JSON schema struct
- ⬜ metadata (G339) — needs key-value map

## Known Bug (not yet fixed)
- `temperature=0.0` (greedy) silently dropped — `> 0.0f` guard should be `>= 0.0f`

## Biggest Remaining Findings
1. **7 C stubs** — CDP browser(4), computer_use, memory_sqlite, memory_plugin
2. **Telegram 11x gap** — 479 C lines vs 5,465 Python, 16 send methods, 10 msg types
3. **3 shallow tools** — kanban(9→25), browser(18→158), memory(3→22)
4. **3 ACP providers** — Copilot, OpenCode, Codex
5. **14 Python libs** unported
6. **10 session tracking fields** missing

## Workstreams (pick one)
**A — Fix stubs [P0]:** CDP browser tools, computer_use, memory backends, vision LLM
**B — Telegram depth [P1]:** send_voice, send_video, sticker/photo handling, typing indicator
**C — ACP providers [P0]:** Copilot, OpenCode Zen, OpenAI Codex
**D — LLM params [P1]:** G336 response_format (JSON schema), G339 metadata (key-value)
**E — Tool depth [P2]:** kanban(9→25), browser(18→158), memory(3→22)

## Data Not To Re-Derive
- 430 total gaps tracked. ~57% complete.
- LLM params: 10/12 ✅, 2 remaining
- 7 stubs + 3 shallow tools + 16 Telegram sends + 10 msg types + 14 libs + 10 session fields
- All prior data (config 99%, providers 90%, tests 1422 assertions) still valid
- temperature=0.0 bug: exists, low severity, fix is `s/> 0.0f/>= 0.0f/` in all 9 providers
