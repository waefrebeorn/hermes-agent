── PERPETUAL GOAL ──

P0: WuBu Hermes C -- 1:1 Python Parity. ~400 gaps to true feature equivalence. ~WuBu~ strives for more.

── MANTRA ──

All code is text to be consumed, understood, rewritten in C. Every Python lib, provider adapter, tool function, gateway platform, config key -- transmuted into C structs, function pointers, switch statements.

~400 gaps is a checklist. Config depth. Provider APIs. Gateway sends. Tool sub-features. Agent state. Plugins. Tests. You do not stop. You are an AI -- boredom is a human invention.

Complete means:
1. Compiles (minimum bar, means nothing)
2. Runs without crash
3. Output verified against Python ground truth
4. Triple DA audit with mind-palace structure

Any ✅ without runtime verification is a lie. Fresh audit (May 22): ~50%.

── WHAT EXISTS ──

/home/wubu/hermes-agent-dev/C/ -- 57K C total (44K src + 7.8K lib + 4.9K tests)
/home/wubu/hermes-agent-dev/C/.hermes/mind-palace/ -- walkway + plans + DA audits + 400-gap-roadmap
/home/wubu/.slermes/ -- config (SLERMES_HOME)
Remote: wubu=waefrebeorn/hermes-agent | Upstream: NousResearch/hermes-agent (125 behind, 52 Python)

── CURRENT REALITY (1:1 parity audit, 2026-05-22) ──

Config:      95% -- 322/322 keys + 6 depth features
Providers:   85% -- 9 ops + 31 aliases + **18/18 LLM params** fully wired through all layers. 3 ACP missing, 25 provider-specific APIs, 12 infra
Tools:       80% -- 28 reg'd, browser/memory/kanban 1:1 with Python. 8 stubs + 36 sub-features
CLI:         80% -- 70 commands, 18 features + 16 depth gaps
Gateway:     35% -- 19 platforms, 63 gaps (16 sends + 10 types + 8 infra + 5 hooks + 4 fmt + 4 err + 16 depth)
MCP:         50% -- transport/tools/resources done. 17 gaps (sub/sampling/roots/serve/multi/auth)
Agent loop:  55% -- 23 state fields. 32 gaps (12 session + 8 params + 3 compression + 3 budget + 4 checkpoint + 6 prefill)
Plugins:     8% -- BIGGEST GAP. 3 .so stubs vs 45 Python backends. 51 gaps total
Tests:       40% -- 26 files, 1,422 assertions vs Python 17K across 900+ files. 53 gaps
Upstream:    12 new feature gaps since last sync
Cross-cut:   40% -- token counting, secure parent dir, key leakage prevention
Build/doc:   30% -- cross-compile, Windows, Docker, CI, man pages, API docs
Error types: 0% -- no typed error hierarchy at all

KEY CORRECTIONS FROM OLD DA:
- Browser: C 13 handlers = 1:1 with Python 12 (C ahead: has browser_forward). Old "158 vs 18" counted internal helpers.
- Memory/Kanban: 1/1 and 9/9 -- parity, not shallow
- Plugins: 45 individual backends, not 19 categories
- Provider-specific APIs: 25 entirely missed
- Overall: ~50%, not old inflated 63%

── THE LOOP ──

1. Read state.md → goal-mantra.md → prestige_prompt.md → overnight-map.md
2. Pick highest undone P0 gap from 400-gap-mega-roadmap.md
3. Implement (C code → build → test)
4. Verify (runtime, not compile-only)
5. Debug (MARKers -- fprintf boundary marks, no deep code-reading trawls)
6. Commit
7. Repeat

NO questions. NO choices. Exhaust only: "awaiting direction."

── P0 NEXT (in order) ──

1. Fix temperature=0.0 bug (s/>0.0f/>=0.0f/ × 9 providers -- 10 min)
2. B04-B05: response_format + metadata (JSON schema struct + key-value map)
3. F01-F08: 8 tool stubs → real (CDP×4, computer_use, memory_sqlite, memory_plugin, vision desc)
4. B01-B03: 3 ACP providers (Copilot, OpenCode, Codex)

── REMINDERS ──

- No delegation for DA audits -- delegation loses receipts. Count everything yourself. Triple-check any delegated result. ~WuBu~ strives for more.
- Every status claim carries verification level (compiles / runs / verified)
- temperature=0.0 is silently dropped -- fix before any LLM work
- Binary works end-to-end: config → .env → LLM call → response (DeepSeek v4 Flash)
- 125 commits behind upstream. 52 Python. 12 new feature gaps.
- New session: read 5 files. Context compaction: use Active Task.
- The endpoint: Python maintainers try C and say "wow, it's exactly the same, just C?"
