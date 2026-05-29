# Slermes C (v143)

Suite: 293/0/0 | Tools: 85 | CLI: 98 | Config sections: 37 | GW: 19 | Prov: 10 | Libs: 65
Binary: 31M | Warnings: 0 | Test files: 250 | C src: 174
Battleship v33 (17 parity gaps across 5 sectors). Fork synced to upstream (0 behind, 0 ahead).

## Fork State
- **Fork**: waefrebeorn/slermes — tracks upstream NousResearch/hermes-agent cleanly
- **Git state**: 0 commits behind upstream, 0 ahead
- **C code**: Tracked in C/ subdirectory, builds independent of Python
- **Old dev branch**: `c-work` preserved on GitHub (277 original commits)

## Progress This Session
- Phase 36: Dead code & warning cleanup in image_routing.c, secrets.c, session_search.c
- Phase 37: Suppressed unused-function/param/variable warnings across 7 files
  (memory.c, config.c, discord.c, kanban.c, computer_use.c, mcp_tool.c, url_safety.c)
  Suite 288/0/0, commits 80a4dc334, b1bfb81b4, 3940341af pushed. Battleship v33 (21→19 gaps). W11 browser UB, W12 strtok_r, W13 missing headers, W14 type mismatches — all resolved.
- Phase 38: ASan build fix (-lz), feishu test segfault fix (http.o→http.c), stale CLI count fix.
  Suite 288/0/0 verified, commits e0d7ccbb0, 86db5fecc pushed.
- Phase 39: Provider mode parity — wired service_tier to Anthropic, reasoning_effort to OpenRouter/Azure/Custom
  Suite 288/0/0, commit 180c0cc97 pushed.
- Phase 40-41: Provider depth + lifecycle hook wiring
  Wired hook_invoke() calls into agent_loop.c at 4 lifecycle points (pre/post LLM, pre/post tool).
  Suite 288/0/0, commit dde46b7c0 pushed.
- Phase 42: Gateway MEDIA support + session meta fields (reasoning_tokens, cost)
- Phase 43: Shell hooks lifecycle wired from YAML config through agent init
- Phase 44: Markdown render test suite (93 tests) + strip link null-termination bug fix
- Phase 45: Tokenizer test suite (79 tests) + float precision bug fix
- Phase 46: ACP permissions test suite (42 tests)
- Phase 47: CLI display test suite (29 tests)
- Phase 48: CLI outcome mapping tests (build_options, build_tool_call, map_outcome)
- Phase 49: Cron scripts tests (10 assertions)
- Phase 50: Scheduler parsing tests (18 assertions)
- Phase 51: Auth store persistence tests (20 assertions)
- Phase 52: ACP resource content-to-text tests (6 assertions)
- Phase 53: Hook & tool result tests, memory leak fixes
  Fixed hook_parse_result context-overrides-block bug (now block > context).
  Fixed agent_free message array leak (512-byte ASan).
  Added test_hook_registry.c (96 assertions) + test_tool_result.c (30 assertions).
  Rewrote/expanded test_title.c, test_lmstudio_reasoning.c, test_trajectory.c.
  Suite 293/0/0. Commits: 1e17559ab, c82d43917, 42e4c92fe, 9bb4eb9f5 pushed.

## Critical Gaps
- **P0**: Form-vs-function (2 gaps) — Python hook, test cheating
- **P1**: Pipeline, cross-comparison (9 gaps)
- **P1**: Upstream drift (1 gap) — test gap U07
- **P2**: Product features (6 gaps)

## Honest Assessment
Real parity gaps exist across form, function, test, pipeline, and upstream drift. Fork synced to upstream.
C/.hermes/ docs tracked in git -- 70+ mind-palace files version-controlled.
