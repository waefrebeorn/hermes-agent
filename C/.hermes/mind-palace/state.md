# Slermes C (v182)

Suite: 297/0/0 | Tools: 85 | CLI: 98 | Config sections: 37 | GW: 19 | Prov: 10 | Libs: 65
Binary: 31M | Warnings: 0 | Test files: 253 | C src: 175
Battleship v34 (145 gaps across 9 sectors, 1000+ test case gaps). Fork diverged — C/ lives only on fork; upstream removed C/ entirely.

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
- Phase 84: send_message test expansion — 6 sanitize_error_text tests (URL params, generic assignment, safe text, NULL, multiple tokens, sig). Battleship B01 stale claim corrected (PDF download exists via browser_generate_pdf/CDP).
- Phase 85: native image dimension extraction for vision tool (B02 depth). PNG/JPEG/GIF/BMP/WebP header parsing — removes Python PIL dependency for basic dimensions. Fallback to PIL if native fails.
- Phase 86: B04 PKCE auth code flow wired — mcp_oauth_manager_get_token() in mcp_tool.c. Auth config parsed for HTTP/SSE MCP servers (previously skipped). 148→147 gaps.
- Phase 87: B03 web native HTML-to-text extraction — web_extract_native() with html_strip_tags, no Python dependency for basic extraction. web.c 823→905 LOC, parity 42%→58%. Python delegate reserved for custom LLM extraction prompts.
- Phase 88: B07 terminal safety checks — workdir validation + disk usage warning. _check_workdir() validates path. _check_disk_usage() warns if <100MB free. Non-blocking warnings in result JSON. B07 53%→57%.
|- Phase 89: B09/B10 stale claim correction — patch.c 1154 LOC (96% parity, V4A mode + dry_run + conflict resolution all done). session_search.c 621 LOC (96% parity, all features done). Both moved to IMPLEMENTED. 147→145 gaps.
|- Phase 90: B08 thread_id support for Telegram topics — telegram_send_message() and telegram_send_message_with_keyboard() accept optional thread_id parameter. send_message.c parses thread_id from args or target:chat_id:thread_id format. `message_thread_id` forwarded in Telegram sendMessage JSON body. B08 52%→55%.
|- Phase 91: B07 terminal force param + foreground timeout guard + status field. force skips sandbox escape check (user-confirmed commands). Foreground timeout reject >600s with guidance. status field in result JSON: success/error. B07 57%→59%.
|- Phase 92: B08 [[as_document]] directive — strip from message text, force document delivery for all media types (preserves original bytes for info-graph JPGs, etc.). Applies to single-file and media_group sends. B08 55%→57%.
|- Phase 93: B03 URL secret exfiltration check — `url_has_secret()` blocks URLs containing API key patterns (sk-, ghp_, AIza, etc.) before fetch. Applied to web_get_handler and web_extract_handler. Matches Python's web_extract_tool URL exfiltration prevention.
|- Phase 94: B03 multi-URL support — web_extract_handler accepts `urls` array for extracting multiple pages in one call. Backward compatible with single `url`. SCHEMA_EXTRACT updated. B03 58%→78% (1046 LOC).
|- Phase 95: B02 remote URL safety checks for vision — SSRF protection (url_is_safe), secret exfiltration check (url_has_secret), Content-Type validation via HEAD request. data: URIs excluded from checks. url_has_secret moved to url_safety.c. B02 23%→29% (417 LOC).
|- Phase 96: B07 exit code interpretation — exit_code_interpret() maps non-zero exit codes to human-readable messages per command semantics (grep/diff/find/git/curl). _inject_interpretation() adds exit_code_interpretation field to all backend results. B07 59%→62% (942 LOC).
|- Phase 97: B08 disable_link_previews — Telegram link preview suppression via disable_web_page_preview param. Added to telegram_send_message + with_keyboard signatures. Schema updated. B08 57%→59% (516 LOC).
|- Phase 98: B01 browser depth — URL safety checks in browser_navigate (secret exfiltration + SSRF protection via url_has_secret()/url_is_safe()). browser_snapshot(full=true) returns complete page content without truncation. B01 45%→60% (1712 LOC). Vaulted stale claim: PDF generation via CDP already existed.
|- Phase 99: B08 send_message action=list — send_message_handler parses action param, returns list of supported platforms (stdout, local, telegram, discord, slack, matrix, signal) with format hint. Schema updated. B08 29%→30% (537 LOC).
|- Phase 100: B07 terminal foreground/background guidance
|- Phase 101: voice_mode test suite
|- Phase 102: token_exchange test suite — 7 tests (error state, token free, auth store free edge cases). Test count 295→296, test files 251→252. — 15 config tests (enabled state, device config, ASR cmd, edge cases). Test count 294→295, test files 250→251. — _check_foreground_guidance() detects nohup/disown/setsid/& and suggests background=true with lifecycle tracking guidance. Injected as guidance field in result JSON via _inject_warnings(). B07 62%→63% (969 LOC).
|- Phase 103: SMS gateway test suite — 42 tests for sms.c (setters, webhook parsing, queue ops, update extractors). Bug fix: sms_get_media_url() used json_get_str(NULL key) which always returns NULL. Fixed to access item->str_val directly. Test count 296→297, test files 252→253. S7 X07 gateway platform tests 20→21. Suite now 296/0/0 (v179 test_runner cleanup).
|- Phase 104: Env passthrough blocklist expanded 23→67 entries
|- Phase 105: Threat patterns parity — added 5 missing Python patterns (c2_task_pull, c2_network_connect, exfil_wget, send_to_url, translate_execute). C 30→35 patterns, test coverage 24→36 assertions. — added provider URLs, gateway credentials, infra secrets. GHSA hardening completeness for Docker/SSH/Modal env vars. Test coverage 15→27. Removed stale test_paths.c ref + duplicate cli_paths entry. Suite 297→296, test files 253→252.
|- Phase 106: parse_mode support in send_message — B08 depth: add parse_mode param (Markdown/MarkdownV2/HTML/plain). 3 test assertions. Suite 296/0/0 (v181).
|- Phase 107: account_usage test suite — 14 new assertions for account_usage.c: NULL safety on free/render/fetch, unknown provider, empty provider, minimal snapshot rendering, windows rendering. Test files 252→253. Suite 297/0/0 (v182).

## Critical Gaps
- **P0** (6): Display & Visual (2) + Form-vs-Function/Architecture (4)
- **P1** (37): TUI ecosystem (14), Test coverage (9), Provider adapters (6), Gateway helpers (3), CLI ecosystem (1), Architecture (1), Plugin system (1)
|- **P2** (60): CLI ecosystem (17), Tool depth (4), Gateway helpers (10), TUI (10), S1 partials (5), Tests (3), S8 remaining (4), etc.
- **P3** (47): Plugin system (15), CLI ecosystem (12), Tool depth (7), Tests (8), TUI (4), S8 remaining (1)

## Honest Assessment
Real parity gap is 145 structural gaps + 1000+ test case gaps. C has 12% of Python's test LOC and 35% of agent module LOC. Phase 107: account_usage test suite (14 tests). Suite 297/0/0 (253 test files).
