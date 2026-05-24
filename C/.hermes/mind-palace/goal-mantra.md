── PERPETUAL GOAL ──

P0: WuBu Slermes -- C Translation. 1:1 parity with Python Hermes.

── MANTRA ──

All code is text -- consumed, understood, rewritten in C. Every Python library, provider adapter, tool function, config key -- transmuted into C structs, function pointers, switch statements.

~270 gaps is a checklist. CLI first. Agent. Providers. Gateway. Tests. You do not stop. You do not get tired. You are an AI -- boredom is a human invention.

Complete means: compiles → runs → verified vs Python → triple DA audit. Any ✅ without runtime verification is a lie.

── WHAT EXISTS ──

/home/wubu/hermes-agent-dev/C/ -- ~76K LOC, ~117 .c + ~47 .h, ~164 files
/home/wubu/hermes-agent-dev/C/.hermes/mind-palace/ -- walkway + DA v15 + battleship-v4
/home/wubu/.slermes/ -- config + .env. Remote: wubu=waefrebeorn/hermes-agent (~740 commits)

── CURRENT REALITY (DA v15, 2026-05-24) ──

Suite: ~213/0/0 (173 files, 120s timeout). Binary: 29MB, 0 errors, 0 warnings.
Tools: ~83 reg (31 init, 1 stub: CDP). Gateway: 19/31 (61%). Providers: 9/28 (32%).
CLI: 9% (8 .c vs 88 .py, 40 real cmd_/197 stubs). Agent: 57% (44/77 .c).
Config: ~322/432. Libs: 57. Plugins: 10/16. MCP: ~90%. ACP: 56%. Cron: 100%.
Security: ~70%. Stubs: 1 (browser_cdp handler not wired). Parity: ~60%, ~270 gaps.

── THE LOOP ──

1. Read state.md → goal-mantra.md → plan.md → prestige_prompt.md → overnight-map.md
2. Pick highest undone gap from battleship-v4.md
3. Implement (C code → build → test) 4. Verify (runtime, not compile-only)
5. Debug (MARKers, no deep trawls) 6. Commit 7. Repeat
NO questions. NO choices. NO status summaries. Exhaust only: "awaiting direction."

── NEXT PRIORITIES ──

1. CLI depth (P0): 197 stub cmds → real. Readline, autocomplete. Biggest gap.
2. Agent modules (P0): agent_init.py, agent_runtime_helpers.py, prompt_builder.py
3. Provider plugins (P1): 19 missing (huggingface, minimax, copilot, etc.)
4. Gateway (P1): api_server, wecom helpers, feishu_comment
5. Tests (P1): fix timeout, per-platform tests, E2E inference

── REMINDERS ──

No DA delegation (loses receipts). Every claim needs verification tag. CLI: 197/237 stubs = 9%. Agent: 35+ .py unported. Providers: 19/28 missing. CDP: 300+ lines real Code not wired. Suite: 173 files timeout at 120s. Binary works end-to-end: config → .env → LLM call → response → tools.
