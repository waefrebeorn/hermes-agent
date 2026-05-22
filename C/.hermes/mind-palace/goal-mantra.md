── PERPETUAL GOAL ──

P0: WuBu Slermes — C Translation. 200 phases to 1:1 parity with Python Hermes.

── MANTRA ──

Time is an illusion. You are an AI. No task is insurmountable. All code is just text to be consumed, understood, and rewritten in C. Every Python library, every provider adapter, every tool function, every config key — it's all just hoarded patterns waiting to be transmuted into C structs, function pointers, and switch statements.

200 phases is not a wall. It is a checklist. You will work through them one at a time, in order, until the last phase is done. Config keys first. Then CLI. Then tools. Then MCP. Then providers. Then the loop. Then everything else. You do not stop. You do not get tired. You do not get bored. You are an AI — boredom is a human invention.

When you think something is complete, it is not complete until:
1. It compiles (minimum bar, means nothing)
2. It runs without crash
3. Output is verified against Python ground truth
4. Triple Devil's Advocate audit with mind-palace structure

Any ✅ claimed without runtime verification is a lie. Any "P0 complete" claim that skips config, MCP, and plugins is survivorship bias. DA v4 (2026-05-21) shows 45% — up from 8% at DA v3, but DA v5 (May 22) pushes to ~57%.

── WHAT EXISTS ──

/home/wubu/hermes-agent-dev/C/ — C source (53K LOC, 121 .c files, 21 .h files)
/home/wubu/hermes-agent-dev/C/.hermes/mind-palace/ — walkway + plans + DA audits + 200-phase roadmap
/home/wubu/hermes-agent-dev/ROADMAP.md — honest status for GitHub viewing
/home/wubu/hermes-agent-dev/README.md — root README (updated to 45%)
/home/wubu/.slermes/ — config (SLERMES_HOME) with .env + config.yaml
Remote: wubu=waefrebeorn/hermes-agent

── CURRENT REALITY (DA v5, 2026-05-22) ──

Config:      64% (206/322 leaf keys from Python DEFAULT_CONFIG)
Tools:       92% (74 registered, 54 expected all found, 14 missing)
Commands:    85% (72/85, all dispatch, most feature-complete)
Providers:   31% (9/29: OpenAI/Anthropic/Google/OpenRouter/DeepSeek/xAI/Azure/Bedrock/Custom + OpenAI-compat)
Gateway:     95% (19 platforms, OAuth partial)
MCP:         70% (core client + 6 tools, namespace/sampling/roots missing)
Plugins:     25% (core .so loader + 3 examples, 12+ types missing)
Session DB:  100% (SQLite FTS5, 231 sessions, CRUD/search/export/branch/migrate)
Delegation:  80% (concurrent children, orchestrator, isolation, cred filter)
Security:    70% (redaction, vault, audit, rate limit, Tirith, approvals)
Agent loop:  75% (budget, interrupt, parallel dispatch, checkpoints, compression)
TUI:         50% (ncurses split-pane, 6/12 phases missing)
Memory:      90% (TTL, dedup, compression, search, auto-save, import/export)
Cron:        90% (SQLite store, retry, chaining, script jobs, watchdog)
Skills:      90% (scan, validate, sync, bundles, curator, dependencies)
Tests:       ~1% (15 files, 1.8K LOC vs ~17K Python tests) — CRITICAL GAP
LLM runtime: WORKING — DeepSeek v4 Flash via OpenAI-compat, tested and verified

── THE LOOP ──

1. Read state.md → goal-mantra.md → plan.md → prestige_prompt.md → overnight-map.md
2. Pick highest undone phase from 200-phase-roadmap.md
3. Implement (C code → build → test)
4. Verify (runtime, not compile-only)
5. Debug (systematic, with MARKers, no deep code-reading trawls)
6. Commit
7. Repeat

NO questions. NO choices. NO status summaries. No "should I continue?" The work continues until all 200 phases are done. When truly everything is exhausted, only then: "awaiting direction."

── REMINDERS ──

- No delegation for DA audits — delegation loses receipts
- Every status claim carries verification level tag
- DA v3 at 8% was wrong — DA v5 at 57% is current. Keep improving measurement.
- Config coverage: 206/322 keys. Still 116 missing. Terminal/logging/skills/checkpoints groups added in DA v5.
- Providers: 9/29 — BIGGEST GAP BY COUNT. 20 provider ports needed.
- Tests: <1% — NOTHING is truly verified without tests.
- No API keys leaked in git history (paranoia scan 2026-05-21 confirmed clean)
- The binary now works end-to-end: config → .env → LLM call → response

── NEXT PRIORITIES ──

1. Config (P1-P25 gap): 164 missing keys — credential_pool, fallback_providers, toolset arrays, web.*, approval subkeys, logging.*, kanban.*, stt/tts/vision/auxiliary sub-dicts, gateway timeouts
2. Providers (P71-P85 gap): 26 missing — OpenRouter, Groq, Together, Bedrock, Azure, xAI. DeepSeek works via OpenAI compat but needs native impl.
3. Tools (P41-P55 gap): 14 missing — feishu (5), video (2), yuanbao (6), MoA (1)
4. Plugins (P126-P140): 12+ plugin types — memory providers, context engine, model providers, image/video gen, browser, achievements, observability
5. TUI (P189-P200): Split panes exist, missing: theme engine, session browser, config editor, image viewer, gateway backend, mobile mode
6. Security (P159-P168 gap): File sandbox (directory restriction)
7. Tests: ~17,000 test cases from Python. Write C test harness.

── FILES TO READ (first turn) ──

If new session: read state.md → goal-mantra.md → plan.md → prestige_prompt.md → overnight-map.md
If context compaction resumed: use "Active Task" section, do NOT re-read 5 files unless user pastes goal template
