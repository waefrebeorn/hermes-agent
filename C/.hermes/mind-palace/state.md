     1|     1|# Slermes C (v139)
     2|     2|
     3|     3|Suite: 289/0/0 | Tools: 85 | CLI: 98 | Config sections: 37 | GW: 19 | Prov: 10 | Libs: 65
     4|     4|Binary: 31M | Warnings: 0 | Test files: 242 | C src: 174
     5|     5|Battleship v33 (17 parity gaps across 5 sectors). Fork synced to upstream (0 behind, 0 ahead).
     6|     6|
     7|     7|## Fork State
     8|     8|- **Fork**: waefrebeorn/slermes — tracks upstream NousResearch/hermes-agent cleanly
     9|     9|- **Git state**: 0 commits behind upstream, 0 ahead
    10|    10|- **C code**: Tracked in C/ subdirectory, builds independent of Python
    11|    11|- **Old dev branch**: `c-work` preserved on GitHub (277 original commits)
    12|    12|
    13|    13|## Progress This Session
    14|    14|- Phase 36: Dead code & warning cleanup in image_routing.c, secrets.c, session_search.c
    15|    15|- Phase 37: Suppressed unused-function/param/variable warnings across 7 files
    16|    16|  (memory.c, config.c, discord.c, kanban.c, computer_use.c, mcp_tool.c, url_safety.c)
    17|    17|  Suite 288/0/0, commits 80a4dc334, b1bfb81b4, 3940341af pushed. Battleship v33 (21→19 gaps). W11 browser UB, W12 strtok_r, W13 missing headers, W14 type mismatches — all resolved.
    18|    18|- Phase 38: ASan build fix (-lz), feishu test segfault fix (http.o→http.c), stale CLI count fix.
    19|    19|  Suite 288/0/0 verified, commits e0d7ccbb0, 86db5fecc pushed.
    20|    20|- Phase 39: Provider mode parity — wired service_tier to Anthropic, reasoning_effort to OpenRouter/Azure/Custom
    21|    21|  Suite 288/0/0, commit 180c0cc97 pushed.
    22|    22|- Phase 40-41: Provider depth + lifecycle hook wiring
    23|    23|  Wired hook_invoke() calls into agent_loop.c at 4 lifecycle points (pre/post LLM, pre/post tool).
    24|    24|  Suite 288/0/0, commit dde46b7c0 pushed.
- Phase 42: Gateway MEDIA: support + session meta fields (reasoning_tokens, cost)
- Phase 43: Shell hooks lifecycle wired from YAML config through agent init
  Commits: 79fe05286, cd5ecfc9c, 34d6ef494. Suite 283/0/0.
  Suite 288/0/0, commits 80fe9dc28, f6df4616d, f754de84b pushed.
- Phase 44: Markdown render test suite (93 tests) + strip link null-termination bug fix
  Wrote test_markdown_render.c covering all render/strip paths.
  Fixed bug in strip link handler: memcpy without null terminator left trailing garbage byte.
  Suite 288/0/0 (+1 test file: 243). Commit f754de84b pushed.
- Phase 45: Tokenizer test suite (79 tests) + float precision bug fix
  Wrote test_hermes_tokenizer.c covering all 7 public functions.
  Fixed float rounding bug in hermes_token_count: (float)(1000/4 + 0.999) = 251, not 250.
  Replaced with integer scaled ceiling division for exact results.
  Suite 288/0/0 (+1 test file: 243). Commit 3c0fda468 pushed.
- Phase 46: ACP permissions test suite (42 tests)
  Wrote test_permissions.c covering all 3 public functions with JSON content validation.
- Phase 47: CLI display test suite (29 tests)
  Wrote test_cli_display.c covering all 17 implemented functions.
  Tests: init, style, panel, HR, printf, cursor, progress bar, spinner.
  Suite 288/0/0 (+1 test file: 243). Commit 50596fec0 pushed.
  Tests: option building (with/without permanent), tool call building (NULL/desc/long/sequential IDs),
  outcome mapping (all 5 mappings, substring prevention, boundary checks).
  Suite 289/0/0 (+1 test file: 243). Commit f01c5fd06 pushed.
  Suite 289/0/0 verified (+2 test files: 245). Phase 49: cron_scripts tests (10 assertions).
  Commit 497127fba pushed.
    25|    25|
    26|    26|## Critical Gaps
    27|    27|- **P0**: Form-vs-function (2 gaps) — Python hook, test cheating
    28|    28|- **P1**: Pipeline, cross-comparison (9 gaps)
    29|    29|- **P1**: Upstream drift (1 gap) — test gap U07
    30|    30|- **P2**: Product features (6 gaps)
    31|    31|
    32|    32|## Honest Assessment
    33|    33|Real parity gaps exist across form, function, test, pipeline, and upstream drift. Fork synced to upstream.
    34|    34|C/.hermes/ docs tracked in git — 70+ mind-palace files version-controlled.
    35|    35|
