# WuBu Slermes C — Goal Mantra (v10 — 2026-05-23)

── PERPETUAL GOAL ──
P0: WuBu Slermes — C Translation. ~286 gaps to 1:1 parity with Python Hermes.

── WHAT EXISTS ──
/home/wubu/hermes-agent-dev/C/ — C source (~380K LOC, 147 source files, 116 test files)
/home/wubu/.slermes/ — config (SLERMES_HOME) with config.yaml + .env

── STATE (DA May 2026) ──
✅ Config: 98% (322 YAML keys, profiles, ${VAR}, !include)
✅ Providers: 87% (9 ops, 10 provider .c — 7 quirks)
✅ MCP: 100%
✅ Plugins: 10/10 .so
✅ Gateway: 100% (19 platforms)
✅ Tools: 95% (28 registered)
✅ Agent: 86% (budget, fallback, checkpoint, interrupt, retry)
✅ CLI: ~148 commands (skin engine, spinner, TUI)
✅ Libs: 100% (30 .a)
⚠️ Tests: 66% (116 files, 154/0/0 suite)
⚠️ Build/doc: 95% (O02 Windows remains)
⚠️ Upstream: 183 commits behind

── BIGGEST GAPS ──
1. Tests (remain ~40+ test files to reach full parity)
2. Upstream catch-up (183 commits behind Python)
3. CLI feel (autocomplete, rich formatting polish)
4. Provider-specific API quirks (7 remaining)

── THE LOOP ──
Read state.md → prestige_prompt.md. Pick highest undone gap.
Implement → build (make) → test (test_runner.sh) → verify → commit.
NO questions. NO choices. Work continues until ~220 gaps closed.

── FULL CONTEXT ──
Read .hermes/mind-palace/prestige_prompt.md
