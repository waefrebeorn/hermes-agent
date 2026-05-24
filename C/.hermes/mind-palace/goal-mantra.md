── PERPETUAL GOAL ──

P0: WuBu Slermes -- C Translation. 200 phases to 1:1 parity with Python Hermes.

── MANTRA ──

All code is text to be consumed, understood, and rewritten in C. Every Python library, provider adapter, tool function, config key -- transmuted into C structs, function pointers, switch statements.

200 phases is a checklist. Config keys first. Then CLI. Tools. MCP. Providers. Loop. Everything else. You do not stop. You do not get tired. You are an AI -- boredom is a human invention.

Complete means:
1. Compiles (minimum bar, means nothing)
2. Runs without crash
3. Output verified against Python ground truth
4. Triple Devil's Advocate audit with mind-palace structure

Any ✅ without runtime verification is a lie. DA v5 (May 22): ~57%.

── WHAT EXISTS ──

/home/wubu/hermes-agent-dev/C/ -- C source (53K LOC, 121 .c, 21 .h)
/home/wubu/hermes-agent-dev/C/.hermes/mind-palace/ -- walkway + plans + DA audits + 200-gap-roadmap
/home/wubu/.slermes/ -- config (SLERMES_HOME) with config.yaml + .env
Remote: wubu=waefrebeorn/hermes-agent

── CURRENT REALITY (DA v5, 2026-05-22) ──

Config:      64% (206/322 leaf keys) -- 116 missing. Auxiliary(56) biggest hole.
Tools:       92% (74 registered, 14 missing: feishu/video/yuanbao/MoA)
Commands:    85% (72/85)
Providers:   31% (9/29: OpenAI/Anthropic/Google/OpenRouter/DeepSeek/xAI/Azure/Bedrock/Custom) -- 20 missing. BIGGEST GAP.
Gateway:     95% (19 platforms), MCP: 70%, Plugins: 25%, Delegation: 80%
Agent loop:  75%, Security: 70%, TUI: 50% (6/12 phases)
Session DB:  100%, Memory: 90%, Cron: 90%, Skills: 90%
Tests:       ~1% (15 files, 1.8K LOC vs ~17K Python) -- CRITICAL GAP
LLM:         WORKING -- DeepSeek v4 Flash via OpenAI-compat

── THE LOOP ──

1. Read state.md → goal-mantra.md → plan.md → prestige_prompt.md → overnight-map.md
2. Pick highest undone gap from 200-gap-roadmap.md
3. Implement (C code → build → test)
4. Verify (runtime, not compile-only)
5. Debug (MARKers, no deep code-reading trawls)
6. Commit
7. Repeat

NO questions. NO choices. NO status summaries. Work continues until all 200 gaps closed. Exhaust only: "awaiting direction."

── NEXT PRIORITIES (highest first) ──

1. Config G1-G11: auxiliary provider sub-configs (56 keys) -- biggest single gap
2. Providers G35-G40: 6 simple OpenAI-compat ports (Nous, Alibaba, StepFun, Minimax, Novita, ZAI)
3. Config G12: TTS config (17 keys)
4. Tools G83-G89: 7 missing (discord, feishu, video, MoA)
5. Tests G161-G165: first 5 test suites (config, providers, CLI, tools, gateway)
6. TUI G132-G137: theme engine, session browser, config editor
7. Security G125-G131: URL safety, command allowlist, rate limiting

── REMINDERS ──

- No delegation for DA audits -- delegation loses receipts
- Every status claim carries verification level tag
- Config: 206/322. Auxiliary(56) biggest gap. Providers: 9/29, 20 missing.
- Tests <1% -- NOTHING is verified without tests.
- Binary works end-to-end: config → .env → LLM call → response
- If new session: read 5 files above. If context compaction: use Active Task.
