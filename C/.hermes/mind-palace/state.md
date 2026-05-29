# Slermes C (v159)

Suite: 294/0/0 | Tools: 85 | CLI: 98 | Config sections: 37 | GW: 19 | Prov: 10 | Libs: 65
Binary: 31M | Warnings: 0 | Test files: 250 | C src: 175
Battleship v34 (148 gaps across 9 sectors, 1000+ test case gaps). Fork diverged — C/ lives only on fork; upstream removed C/ entirely.

## Fork State
- **Fork**: waefrebeorn/slermes — tracks upstream NousResearch/hermes-agent
- **Git state**: Fork diverged — 90 commits ahead of upstream (upstream deleted C/ entirely)
- **C code**: Tracked in C/ subdirectory, builds independent of Python
- **Old dev branch**: `c-work` preserved on GitHub (277 original commits)

## Progress This Session
- Phase 55: Upstream rebase + full doc sweep (v144→v145)
- Phase 56: Vaulted battleship v33 (17 gaps, too narrow). Created battleship v34 with real 7-axis parity audit
- Phase 57: S0 stale sweep — 16 claims retired
- Phase 58: S1 stale sweep — 7 claims retired + L14/L03/L09/L10
- Phase 59: S2 stale sweep — 30 claims retired, S2 45→18
- Phase 60: B09 dry_run + S6 stale sweep — 5 claims retired (B05/B06/B08/B10)
- Phase 61: Surrogate sanitization wired. env_passthrough+config bridged
- Phase 62: session_search scroll+browse modes. Feature-gap methodology documented
- Phase 64: error_classify() wired into LLM client (structured error logging + compress/rotate hints). S2 stale sweep — A15 (insights) retired, already ported as usage_pricing.c + cmd_insights
- Phase 65: Upstream header capture (cf-ray, x-openrouter-*, x-request-id) wired into non-streaming LLM path. Structured stream error logging with provider/model.
- Phase 66: Stream drop diagnostics — elapsed time, token count, TTFB included in error messages on both streaming paths.
- Phase 67: Model management CLI — /model list, show, providers, set. A18 ported to PARTIAL.
- Phase 72: Terminal env passthrough wired (B07 — build_env_passthrough_export, integrated into command builder). Stale claims corrected: B08 inline_buttons/reply_to already implemented, B10 filters already implemented.
- Phase 73: libmcp_oauth integrated into mcp_tool.c (B04) — token storage via mcp_oauth_storage, PKCE fields (authorization_url, redirect_uri) parsed from YAML config. test_runner.sh updated for mcp_oauth + crypto + base64 deps.
- Phase 74: patch conflict resolution (B09) — when old_string found multiple times, returns JSON with offset + context snippet for each match instead of bare error
- Phase 75: FTS5 query syntax for session_search (B10) — AND/OR/"phrase"/-exclude parsing, multi-term scoring, windowed snippet extraction
- Phase 76: Telegram media group support (B08) — media_group array param, builds InputMedia array, sends via telegram_send_media_group()
- Phase 77: A22 stream diag — upstream header capture wired in streaming path (provider + legacy). http_stream_request now accumulates response headers in http_t.resp_headers. Headers captured before http_free(h) on both streaming paths.
- Phase 78: A22 stream diag — user-facing inline notification. Logs `[llm] upstream=[cf-ray=... x-openrouter-*]` on stream success. A22 PORTED.
- Phase 79: A18 models.dev — models_dev_fetch() with 3-tier cache (in-memory/disk/network). models_dev_lookup_context(). models_dev_list_json(). A18 PORTED. S2 all real gaps resolved.
- Phase 80: B03 web tool depth — web_get save_to-file mode (save_path param). Binary/PDF download support. 151→150 gaps.
- Phase 81: yuanbao_tools dangling pointer bug fix + 21-test suite. 3 use-after-free bugs fixed.
- Phase 82: libtooloutput 23-test suite (env vars, defaults, boundary checks). Test count 295→294 (duplicate tool_output entry removed). Test files 249→250. 149→148 gaps.
- Phase 83: send_message error redaction (B08 depth). Port of Python _sanitize_error_text — redacts secrets (access_token, api_key, token, sig) from error messages before surfacing to users/models.

## Critical Gaps
- **P0** (6): Display & Visual (2) + Form-vs-Function/Architecture (4)
- **P1** (37): TUI ecosystem (14), Test coverage (9), Provider adapters (6), Gateway helpers (3), CLI ecosystem (1), Architecture (1), Plugin system (1)
- **P2** (63): CLI ecosystem (17), Tool depth (7), Gateway helpers (10), TUI (10), S1 partials (5), Tests (3), S8 remaining (4), etc.
- **P3** (47): Plugin system (15), CLI ecosystem (12), Tool depth (7), Tests (8), TUI (4), S8 remaining (1)

## Honest Assessment
Real parity gap is 148 structural gaps + 1000+ test case gaps. C has 12% of Python's test LOC and 35% of agent module LOC. Phase 81: yuanbao_tools bug fix + test suite.
