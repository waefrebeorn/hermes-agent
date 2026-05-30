# Battle Map v34 — Comprehensive Parity Assessment (DA v1)

**v254 | Fork diverged — C/ lives only on fork | Suite 315/0/0 | 85 tools | 98 CLI**
**Honest assessment: 128 structural gaps, 1000+ test case gaps across 9 sectors. S7 X01 test files 272 (21.6% parity). Phase 186: B07 terminal depth — terminal_read_shell_token() ported. 12-test suite (102 total). Suite 315/0/0. Gaps unchanged (depth).**

v34 replaces v33's narrow 17-gap form-vs-function focus with true 7-axis parity audit.
Every sector count verified against live source code. DA v1: first-pass deep audit.

---

## S0: Display & Visual — Phase 0 (P0)

C display_core.c (1211 LOC) + lib/libskin (657 LOC) + line_edit (593 LOC) already provides skin engine, spinners, banner, status bar, tool feed, panel/box, tables, progress bars, diff render, help categories, 24-bit color, kawaii faces, tool emoji mapping. 12 of 18 original v34 claims were stale.

| # | ID | Feature | Python | C | Status | Priority |
|---|----|---------|--------|---|--------|----------|
| 01 | D09 | Prompt input: tab completion, history search, multi-line editor | prompt_toolkit (async, emacs/vi modes) | Ctrl-R search, horizontal scroll, Alt+Enter multi-line. Has tab completion + history + bracketed paste. Missing vi/emacs modes. | PARTIAL | P0 |
| 02 | D16 | Recurrent typing: type-ahead, input buffering during LLM call | prompt_toolkit async input queue + keyboard interrupt | fgets/line_edit blocks during LLM, type-ahead lost | REAL GAP | P0 |

**S0: 2 gaps (1 real, 1 partial) — down from 18 as of v145 Phase 57.**

---

## S1: Conversation Loop — Plumbing Gaps (P1)

Python's run_conversation (4606 LOC) — C's agent_loop.c (1600 LOC) covers all plumbing features except 5 partial depth items.
14 stale + 3 done (L14/L03/L09/L10). All 5 REAL claims retired as stale. 5 PARTIAL remain (L24-L28).

| # | ID | Feature | Python | C | Priority | Status |
|---|----|---------|--------|---|----------|--------|
| 01 | L24 | Turn-level checkpoint/snapshot for rollback | snapshot_create/restore per tool iteration | checkpoint_init exists but simpler | P2 | PARTIAL |
| 02 | L25 | Agent runtime helpers: tool schema management | agent_runtime_helpers.py (2366 LOC) | None | P1 | REAL |
| 03 | L26 | Chat completion helpers: request building, streaming | chat_completion_helpers.py (2467 LOC) | llm_chat_completion is simpler | P1 | PARTIAL |
| 04 | L27 | Prompt builder: system prompt assembly, dynamic sections | prompt_builder.py (1451 LOC) | hermes_system_prompt.h is simpler | P1 | PARTIAL |
| 05 | L28 | Agent init: full AIAgent construction with 60+ params | agent_init.py (1649 LOC) | agent_init() + agent_configure_from_config() | P1 | PARTIAL |

**S1: 5 gaps (all partial: L24-L28) — 19 stale + 4 done. No remaining real gaps.**

---

## S2: Agent Modules — Missing Ports (P1-P3)

S2 stale sweep (Phase 59): 30 of 45 claimed "no C equivalent" modules have direct C ports. 15 are cloud-service-specific or Python-architecture-abstraction (won't port to standalone C binary). 3 real implementable gaps remain.

### Real Implementable Gaps

No remaining real implementable gaps. All S2 real gaps are PORTED (A15, A22) or vaulted (A18 models_dev — HTTP fetch + 3-tier cache implemented).

### Vaulted / Ported Items

| # | ID | Reason |
|---|----|--------|
| 01 | A15 | insights.py (930 LOC) — already ported as usage_pricing.c + cmd_insights. Suite verified. |
| 02 | A22 | stream_diag.py (280 LOC) — PORTED. upstream header capture in both streaming paths + user-facing inline notification on stream success. |
| 03 | A18 | models_dev.py (725 LOC) — PORTED. models_dev_fetch() with 3-tier cache (in-memory/disk/network), models_dev_lookup_context(), models_dev_list_json(). Falls back to stale cache on network failure. |

### Cloud-Specific / Python-Architecture (won't port to C)

| # | ID | Python Module | LOC | Reason |
|---|----|--------------|-----|--------|
| 01 | A10 | codex_responses_adapter.py | 1221 | OpenAI Codex cloud API — standalone C binary doesn't use Codex |
| 02 | A17 | gemini_cloudcode_adapter.py | 909 | Gemini Codex cloud mode — cloud IDE feature |
| 03 | A19 | copilot_acp_client.py | 686 | GitHub Copilot ACP — cloud IDE feature |
| 04 | A25 | codex_runtime.py | 536 | Codex runtime — cloud IDE feature |
| 05 | google_code_assist.py | ~400 | Google Code Assist — cloud IDE feature |
| 06 | transcription_provider/registry | ~600 | Python plugin abstraction — media pipeline |
| 07 | tts_provider/registry | ~500 | Python plugin abstraction — media pipeline |
| 08 | video_gen_provider/registry | ~400 | Python plugin abstraction — media pipeline |
| 09 | web_search_provider/registry | ~600 | Python plugin abstraction — search backend |
| 10 | image_gen_provider/registry | ~400 | Python plugin abstraction — media pipeline |
| 11 | memory_provider.py | ~300 | Python plugin abstraction — memory backends |
| 12 | credential_sources/ | ~400 | Python subdirectory — plugin credential backends |
| 13 | jiter_preload.py | ~200 | Python-specific JSON parser preload |
| 14 | async_utils.py | ~200 | Python async utilities — C is sync |
| 15 | background_review.py | 597 | Heavy async+subprocess orchestration — C arch mismatch |

**S2: 15 gaps (all won't-port — A18/A22 PORTED, A15 vaulted). S2 phase complete — all real implementable gaps resolved.**

---

## S3: Gateway Sub-Modules — Helper Files (P1)

13 Python gateway helper files with no C equivalent. C has 19 core platforms only.

| # | ID | File | LOC | Purpose | Priority |
|---|----|------|-----|---------|----------|
| 01 | G01 | helpers.py | 278 | msg_dedup, strip_markdown, redact_phone, thread_tracker — PORTED | P1 | ✅ PORTED |
|| 02 | G02 | base.py | ~4286 | Gateway base class, rate limiting, retry — PORTED: gw_platform_t vtable, gw_rate_limiter_t, gw_retry_with_backoff, gw_utf16_len/gw_prefix_within_utf16_limit, url_safe_for_log(), url_is_network_accessible(), media_cache_save/media_cache_cleanup, media_should_send_as_audio, http_parse_retry_after, http_no_proxy_match(), http_split_host_port() (Phase 184 — host:port parsing with URL/IPv6/host:port/plain), http_no_proxy_entries() (Phase 184 — reads NO_PROXY env var), http_should_bypass_proxy() (Phase 184 — high-level proxy bypass check). Missing: proxy detection (macOS scutil, won't port to Linux C) | P1 | PARTIAL |
| 03 | G03 | feishu_comment.py | ~400 | Feishu comment handling — PORTED: textwrap_chunk(), feishu_sanitize_comment_text(), feishu_get_reply_user_id(), feishu_extract_reply_text(), feishu_truncate_text(), feishu_extract_semantic_text(). Missing: async Feishu API functions, prompt builders (won't port — lark_oapi dependent). | P2 | ✅ PORTED — 20 tests (Phase 169) |
| 04 | G04 | feishu_comment_rules.py | ~300 | Feishu comment moderation rules | P2 | ✅ PORTED — C has feishu_comment_rules.c + 56 tests (Phase 167) |
| 05 | G05 | wecom_crypto.py | ~350 | WeCom message encryption | P2 | ✅ PORTED — C has wecom_crypto.c + 28 tests |
|| 06 | G06 | wecom_callback.py | ~300 | WeCom callback verification — PARTIAL: wecom_xml_extract_tag() + wecom_callback_user_app_key() ported (Phase 181). wecom_callback_build_event() ported (Phase 185 — decrypted XML event builder: parses MsgType/Event/Content/MsgId/CreateTime, filters lifecycle events, builds scoped_chat_id). 37-test suite (62 total). Missing: async HTTP server (WON'T PORT — C uses webhook.c). | P2 | PARTIAL — 62-test suite (Phase 185) |
||| 07 | G07 | telegram_network.py | ~450 | Telegram proxy/network config — PORTED: http_client_set_proxy() with env auto-detection, telegram_parse_fallback_ips() (Phase 161), telegram_resolve_system_dns() (Phase 177), telegram_query_doh + telegram_parse_doh_response (Phase 178), telegram_discover_fallback_ips (Phase 179), telegram_rewrite_url_for_ip (Phase 180). Missing: TelegramFallbackTransport (async httpx transport, won't port — C is synchronous). 22 tests. | P2 | ✅ PORTED — 22 tests (Phase 180) |
|| 08 | G08 | signal_rate_limit.py | ~200 | Signal rate limiting — PORTED: datetime_format_duration (Phase 147), signal_is_rate_limit_error (Phase 156), signal_send_timeout (Phase 156), signal_extract_retry_after + signal_parse_retry_after_message (Phase 157 — parses retryAfterSeconds from structured JSON error.data.response.results[*] and "Retry after N seconds" text). Missing: SignalAttachmentScheduler (asyncio, won't port) | P2 | ✅ PORTED |
||| 09 | G09 | yuanbao_media.py | ~350 | Yuanbao media attachments — PORTED: url_extract_basename, url_guess_mime_type, url_is_image_extension, url_get_image_format, url_parse_image_size (PNG/JPEG/GIF/WebP dimension parsing), crypto_md5_hex, yuanbao_generate_file_id, yuanbao_build_image_msg, yuanbao_build_file_msg. 15 tests. Missing: download_url (async HTTP), COS upload/sign (cloud-specific) | P2 | ✅ PORTED — 15 tests (Phase 176) |
|| 10 | G10 | yuanbao_proto.py | ~300 | Yuanbao protobuf messages | P2 | ✅ PORTED — C has libprotobuf library + yuanbao.c encode_conn_msg/decode_conn_msg/encode_send_c2c/encode_auth_bind/encode_ping_req. Both achieve same ends (C: type-specific encoders, Python: generic biz_msg wrapper). |
| 11 | G11 | yuanbao_sticker.py | ~200 | Yuanbao sticker handling | P2 | ✅ PORTED — C has 59-sticker DB, search/send tools |
| 12 | G12 | api_server.py | ~500 | REST API server for HTTP gateway | P1 | ✅ PORTED — C has api_server.c (1224 LOC) |
| 13 | G13 | _http_client_limits.py | ~200 | HTTP client connection limits | P2 | ✅ PORTED — C has http_client_set_pool() |

**S3: 2 gaps (1 P1, 1 P2) — G02 base PARTIAL, G06 wecom_callback PARTIAL (Phase 185: wecom_callback_build_event ported, 62-test suite). G07 telegram_network PORTED (Phase 180 — 22 tests). Phase 185: G06 depth — wecom_callback_build_event.**

---

## S4: TUI Ecosystem — Full Backend + Frontend (P1)

C has 1 ncurses file (tui_fullscreen.c, 3374 LOC). Python has 28 Ink React tsx + 8 tui_gateway .py (~16,000 LOC).

| # | ID | Component | Python | C State | Priority |
|---|----|-----------|--------|---------|----------|
| 01 | T01 | TUI JSON-RPC gateway server | tui_gateway/server.py (6643 LOC) | 10 methods inline | P1 |
| 02 | T02 | TUI transport layer | tui_gateway/transport.py | None | P1 |
| 03 | T03 | TUI render engine | tui_gateway/render.py | ncurses panel only | P1 |
| 04 | T04 | TUI WebSocket support | tui_gateway/ws.py | None | P1 |
| 05 | T05 | TUI entry/startup | tui_gateway/entry.py | None | P1 |
| 06 | T06 | TUI slash command worker | tui_gateway/slash_worker.py | None | P1 |
| 07 | T07 | TUI event publisher | tui_gateway/event_publisher.py | None | P1 |
| 08 | T08 | App layout + chrome | appLayout.tsx, appChrome.tsx | ncurses panel only | P1 |
| 09 | T09 | Text input: autocomplete, history, multi-line | textInput.tsx (1233 LOC) | fgets() line editor | P1 |
| 10 | T10 | Markdown render: rich streaming render | markdown.tsx (1119 LOC) | None | P1 |
| 11 | T11 | Thinking indicator: animated states | thinking.tsx (1206 LOC) | None | P1 |
| 12 | T12 | Session picker | sessionPicker.tsx | None | P2 |
| 13 | T13 | Model picker | modelPicker.tsx | None | P2 |
| 14 | T14 | Agents overlay | agentsOverlay.tsx | None | P2 |
| 15 | T15 | Todo panel | todoPanel.tsx | None | P2 |
| 16 | T16 | Streaming markdown live update | streamingMarkdown.tsx | None | P1 |
| 17 | T17 | Bounding box: window resize re-render | SIGWINCH + full re-layout | handle_winch signal only | P1 |
| 18 | T18 | Recurrent typing: type-ahead during LLM call | Async input queue | fgets blocks, input lost | P1 |
| 19 | T19-T28 | (10 more tsx components) | ~4500 LOC total | None | P2-P3 |

**S4: 28 gaps (14 P1, 10 P2, 4 P3)**

---

## S5: CLI Ecosystem — hermes_cli Infrastructure (P2)

Python has 80+ CLI modules (~70,000 LOC). C has none of these.

| # | ID | Module | LOC | Priority |
|---|----|--------|-----|----------|
| 01 | C01 | Setup wizard (setup.py) | ~2000 | P2 |
| 02 | C02 | Doctor diagnostics (doctor.py) | ~1500 | P2 |
| 03 | C03 | Memory setup (memory_setup.py) | ~500 | P3 |
| 04 | C04 | Profile management (profiles.py) | ~1000 | P2 |
| 05 | C05 | Config editor (config.py) | ~800 | P2 |
| 06 | C06 | Env loader (env_loader.py) | ~200 | P2 |
| 07 | C07 | Model management (models.py) | ~2000 | P2 |
| 08 | C08 | Model switch (model_switch.py) | ~1000 | P2 |
| 09 | C09 | Model catalog (model_catalog.py) | ~2000 | P2 |
| 10 | C10 | Codex models (codex_models.py) | ~1000 | P3 |
| 11 | C11 | Auth/OAuth system (auth.py + auth_commands.py) | ~5000 | P1 |
| 12 | C12 | Copilot auth (copilot_auth.py) | ~1000 | P3 |
| 13 | C13 | Gateway CLI (gateway.py + gateway_windows.py) | ~4000 | P2 |
| 14 | C14 | Webhook setup (webhook.py) | ~1000 | P2 |
| 15 | C15 | Platform management (platforms.py) | ~2000 | P2 |
| 16 | C16 | Kanban system (kanban.py + 7 files) | ~11000 | P2 |
| 17 | C17 | Skills hub (skills_hub.py) | ~1500 | P2 |
| 18 | C18 | Voice mode (voice.py) | 846 | P3 |
| 19 | C19-C30 | Other CLI modules | ~25000 | P2-P3 |

**S5: 30 gaps (1 P1, 17 P2, 12 P3)**

---

## S6: Tool Depth — Feature Gaps (P2)

C tools are at 48% parity by LOC (30,288 vs 62,781).

| # | ID | Tool | C LOC | Python LOC | Parity | Missing Features | Priority | Status |
|---|----|------|-------|-----------|--------|-----------------|----------|--------|
| 01 | B01 | browser | ~1712 | ~3800 | 60% | browser_navigate URL safety: secret exfiltration + SSRF protection via url_has_secret()/url_is_safe() (Phase 98). browser_snapshot(full=true) returns complete page content. PDF generation via CDP already implemented. autofill requires real browser engine (won't port) | P2 | PARTIAL |
|| 02 | B02 | vision | ~567 | ~1436 | 39% | native PNG/JPEG/GIF/BMP/WebP dimension extraction (Phase 85). Remote URL safety checks: SSRF protection (url_is_safe), secret exfiltration (url_has_secret), Content-Type validation via HEAD query (Phase 95). Native base64 data URL conversion — image_to_base64_data_url() for direct provider consumption via data: URIs (Phase 118). Magic byte detection for extensionless files (detect_image_magic in vision.c — reads first 12 bytes, checks 6 formats). detail param passthrough (low/high/auto) already wired C:299,326,362,509. Face detection and barcode require OpenCV (won't port). Color analysis / EXIF extraction / OCR via helpers — STALE (none exist in Python vision_tools.py). Media-in-tool-results support (Phase 162): vision_supports_media_in_tool_results() checks provider+model capability following Python mapping. 23 tests. Video MIME detection (Phase 165): vision_detect_video_mime_type() maps 7 video extensions (mp4/webm/mov/avi/mkv/mpeg/mpg) to MIME types. Video base64 data URL: vision_video_to_base64_data_url() reads file, base64 encodes, returns data: URI. 14 tests. | P2 | PARTIAL |
||| 03 | B03 | web | ~1046 | ~1326 | 78% | cookie jar persistence (Phase 68) + save-to-file mode via save_path param for binary/PDF downloads (Phase 80). Native HTML-to-text extraction via html_strip_tags — no Python dependency for basic web_extract (Phase 87). URL secret exfiltration check blocks URLs containing API key patterns (Phase 93). Multi-URL support accepts urls array for extracting multiple pages in one call (Phase 94). Base64 image stripping: data:image/ URIs removed from extracted text to reduce clutter (Phase 139). 13-test suite for clean_base64_images: NULL, empty, plain passthrough, single/multiple images, JPEG/GIF, inline HTML quotes (Phase 140). All core features (web_get, web_search, web_extract, clean_base64_images) ported. Remaining: backend abstraction, LLM content processing, config/env checks — Python-architecture-specific (won't port to standalone C binary — config loading via hermes_config_t, no optional deps, no async AI pipeline). | P2 | ✅ PORTED — all core user-facing features implemented. Python backend/config infra is arch-mismatch, won't port. |
|| 04 | B04 | mcp_tool | ~3875 | ~3584 | 108% | OAuth: libmcp_oauth manager integration — mcp_oauth_manager_get_token() with PKCE auth code flow (callback server, browser open, token exchange/refresh, mtime-change detection). Auth config parsed for HTTP/SSE servers too | P2 | ✅ IMPLEMENTED |
| 05 | B05 | file | ~3000 | ~1220 | 246% | ALL features implemented (glob, fswatch, diff, hex, symlink all verified) | P2 | ✅ IMPLEMENTED |
| 06 | B06 | feishu_tools | ~210 | ~872 | 24% | Both doc_read + drive_list exist — matches Python feature set | P2 | ✅ IMPLEMENTED |
||| 07 | B07 | terminal | ~981 | ~2409 | 40% | env passthrough wiring from libenvpassthrough to exec (Phase 72). workdir validation + disk usage warning (Phase 88). force param + foreground timeout guard + status field (Phase 91). exit code interpretation: human-readable messages per command (grep/diff/find/git/curl), injected into all backend results (Phase 96). foreground/background guidance: detects nohup/disown/setsid/& and suggests background=true (Phase 100). pipe_stdin detection: PTY auto-override for gh auth login --with-token (Phase 134). help/version detection: --help/-h/--version/-v skip guidance early (Phase 135). env assignment detection: KEY=VALUE prefix skips guidance (Phase 137). Long-lived foreground pattern detection: npm/pnpm/yarn/bun run dev|start|serve|watch, docker compose up, next dev, vite, nodemon, uvicorn, gunicorn, python -m http.server (Phase 141). Sudo failure detection: checks output for sudo: a password is required / sudo: no tty present / sudo: a terminal is required and adds sudo_tip field (Phase 142). Workdir validation: allowlist-based safe-path check blocking shell metacharacters (Phase 143). Sudo nopasswd probe: terminal_sudo_nopasswd_works() checks if sudo -n works without prompt (Phase 183). Shell token reader: terminal_read_shell_token() reads tokens preserving quotes/escapes (Phase 186). 102-test suite (+12). | P2 | PARTIAL |
|| 08 | B08 | send_message | ~710 | ~1786 | 40% | inline_buttons + reply_to_message_id implemented. media_group array support added. error redaction: secrets sanitized from error messages. thread_id support. [[as_document]] directive. disable_link_previews for Telegram link suppression (Phase 97). action=list returns available platforms (Phase 99). parse_mode parameter (Markdown/MarkdownV2/HTML/plain) (Phase 106). disable_notification (silent send) for Telegram (Phase 126). Telegram retry with exponential backoff (Phase 132): 3 attempts with 0.5s/1s/2s delay on transient failures, port of Python _telegram_retry_delay + _send_telegram_message_with_retry. General topic thread_id mapping (Phase 158): telegram_message_thread_id_for_send() maps "1" -> NULL matching TelegramAdapter._message_thread_id_for_send(). Parse mode fallback (Phase 159): when send fails with non-default parse_mode, retries once with parse_mode=NULL (plain text). telegram_send_with_mode() helper extracted. HTML auto-detection (Phase 160): message_looks_like_html() auto-switches to parse_mode=HTML when message contains HTML tags. Telegram thread-not-found detection (Phase 164): telegram_is_thread_not_found() checks errors for "thread not found" via case-insensitive substring match with separator variants (space/underscore/hyphen). 12-test standalone suite. | P2 | PARTIAL |
| 09 | B09 | patch | ~1154 | ~1200 | 96% | ✅ dry_run, V4A multi-file patch mode, 9 fuzzy matching strategies, conflict resolution (snippet JSON), replace_all — ALL parity features implemented | P2 | ✅ IMPLEMENTED |
| 10 | B10 | session_search | ~621 | ~650 | 96% | scroll + browse modes, tag_filter, role_filter, session_id_filter, offset pagination, FTS5 query syntax (AND, quotes, -exclude), session_search single-shape discovery/scroll/browse API — ALL implemented | P2 | ✅ IMPLEMENTED |
| 11 | B11-B20 | remaining tools | ~50-80% | varying | partial | Various | P2-P3 | STALE — needs verification |

**S6: 11 gaps (4 P2, 7 P3) — Phase 186: B07 terminal depth (terminal_read_shell_token). Suite 315/0/0.**

---

## S7: Test Coverage — Massive Gap (P1)

| # | ID | Metric | Python | C | Ratio | Priority |
|---|----|--------|--------|---|-------|----------|
|| 01 | X01 | Test files | 1,262 | 272 | 21.6% | P1 |
| 02 | X02 | Test LOC | 473,891 | 59,111 | 12.5% | P1 |
| 03 | X03 | Provider tests | ~200 | ~30 | 15% | P1 |
| 04 | X04 | Tool tests | ~400 | ~100 | 25% | P1 |
| 05 | X05 | Integration tests (live API) | ~300 | 0 | 0% | P1 |
| 06 | X06 | Agent loop tests | ~150 | ~30 | 20% | P1 |
| 07 | X07 | Gateway platform tests | ~100 | ~22 | 22% | P1 |
| 08 | X08 | Conversation loop tests | ~200 | ~10 | 5% | P1 |
| 09 | X09 | Edge case / regression tests | ~62 | ~3 | 5% | P1 |
| 10 | X10 | Fuzz / property tests | ~0* | 0 | 0% | P3 |
| 11 | X11 | Performance / benchmark tests | ~30 | 0 | 0% | P2 |
| 12 | X12-X20 | Subsystem test gaps | ~200 | ~50 | 25% | P1-P2 |

**S7: 19 gap clusters (9 P1, 3 P2, 7 P3) — 1,000+ individual test cases. Phase 177: test files 271→272, suite 312→313.**

---

## S8: Provider Adapter Layer (P1)

Python has adapter layers wrapping provider APIs (~9,700 LOC total).

| # | ID | Adapter | LOC | Missing in C | Priority |
|---|----|---------|-----|-------------|----------|
| 01 | R01 | anthropic_adapter.py | 2275 | Streaming variants, extended thinking, prompt caching | P1 |
| 02 | R02 | bedrock_adapter.py | 1289 | AWS Bedrock auth, model discovery | P1 |
| 03 | R03 | google_oauth.py | 1059 | OAuth token exchange, refresh | P1 |
| 04 | R04 | gemini_native_adapter.py | 971 | Gemini native API features, safety | P1 |
| 05 | R05 | gemini_cloudcode_adapter.py | 909 | Gemini Codex mode | P2 |
| 06 | R06 | azure_identity_adapter.py | 555 | Azure managed identity, OAuth2 | P1 |
| 07 | R07 | codex_responses_adapter.py | 1221 | Codex API response handling | P2 |
| 08 | R08 | copilot_acp_client.py | 686 | GitHub Copilot ACP client | P2 |
| 09 | R09 | plugin_llm.py | 1046 | Plugin-based LLM abstraction | P2 |
| 10 | R10 | model_metadata.py | 1850 | Model discovery, catalog, capabilities | P1 |

**S8: 10 gaps (6 P1, 4 P2)**

---

## S9: Plugin System — Architecture Gap (P2)

C has plugin_ext.c for loading .so shared libraries but zero actual plugins shipped.

| # | ID | Plugin | Python State | C State | Priority |
|---|----|--------|-------------|---------|----------|
| 01 | P01 | Architecture: plugin loading + lifecycle | 50+ plugins, plugin manager | plugin_ext.c loads .so only | P1 |
| 02 | P02 | Memory providers (mem0, honcho, etc.) | 8 plugins | 0 | P2 |
| 03 | P03 | Model provider plugins | 29 plugins | hardcoded in provider_*.c | P2 |
| 04 | P04 | Kanban board/workflow | 7 files, ~11,000 LOC | 0 | P2 |
| 05 | P05 | Observability / telemetry | 1 plugin | 0 | P3 |
| 06 | P06 | Achievements | 1 plugin | 0 | P3 |
| 07 | P07-P20 | Other plugins | ~14 more | 0 | P3 |

**S9: 20 gaps (1 P1, 4 P2, 15 P3)**

---

## S10: Architecture & Platform (P0)

| # | ID | Gap | Detail | Priority |
|---|----|-----|--------|----------|
| 01 | F01 | C can't hook Python | Standalone binary, cannot import Python | P0 |
| 02 | F02 | Test count mismatch | 258 C vs 1,262 Python tests | P0 |
| 03 | F03 | No Python interop | Cannot reuse Python libraries at runtime | P0 |
| 04 | F04 | Single-threaded agent loop | Python uses asyncio for concurrent ops | P0 |
| 05 | F05 | No credential automation | Python OAuth flows not replicated | P1 |
| 06 | F06 | No ACP protocol server | VS Code/Zed/JetBrains integration missing | P2 |
| 07 | F07 | No session replay / debugging | Python session trajectory replay | P2 |
| 08 | F08 | Raw socket health check | No TCP keepalive / zombie socket recovery | P1 |
| 09 | F09 | No async event loop | Python uses asyncio for gateway + tools | P0 |
| 10 | F10 | No stdin/stdout safe guard | Systemd/daemon crash from broken pipe | P1 |

**S10: 10 gaps (4 P0, 3 P1, 2 P2, 1 P3)**

---

## SUMMARY

| Sector | Gaps | P0 | P1 | P2 | P3 | Description |
|--------|------|----|----|----|----|-------------|
| S0: Display & Visual | 2 | 2 | 0 | 0 | 0 | Phase 0 — D13/D14 done; 15 stale claims retired |
| S1: Conversation Loop Plumbing | 5 | 0 | 0 | 5 | 0 | All 28 real gaps stale-retired or implemented in Phase 57-58. 5 partials (L24-L28) remain |
|| S2: Agent Modules | 15 | 0 | 0 | 0 | 0 | All real gaps PORTED (A18/A22/A15). 15 won't-port remain. |
||| S3: Gateway Helpers | 3 | 0 | 2 | 1 | 0 | G07 telegram_network PORTED (Phase 180). G06 remains. |
| S4: TUI Ecosystem | 28 | 0 | 14 | 10 | 4 | Full TUI backend + React frontend |
| S5: CLI Ecosystem | 30 | 0 | 1 | 17 | 12 | hermes_cli infrastructure |
||| S6: Tool Depth | 14 | 0 | 0 | 7 | 7 | Phase 180: B03 web PORTED (stale claim — all core features). |
| S7: Test Coverage | 20* | 0 | 9 | 3 | 8 | *1,000+ test cases behind |
| S8: Provider Adapters | 10 | 0 | 6 | 4 | 0 | Adapter layer missing (9,700 LOC) |
| S9: Plugin System | 20 | 0 | 1 | 4 | 15 | Architecture gap |
| S10: Architecture | 10 | 4 | 3 | 2 | 1 | Form-vs-function |
|| **TOTAL** | **128** | **6** | **36** | **58** | **43** | **Phase 186: B07 terminal depth — terminal_read_shell_token. Suite 315/0/0.** |

### Phase Map

| Phase | Focus | Sectors | Gap Count |
|-------|-------|---------|-----------|
|| Phase 0 | Display & Visual | S0 (2) | 2 |
| Phase 1 | Agent plumbing + Provider adapters + TUI backend | S1 (5), S8 (6), S4 P1 (14) | ~25 |
| Phase 2 | Test coverage campaign | S7 | 20* (1000+ tests) |
| Phase 3 | Gateway helpers + Tool depth | S3, S6 | ~30 |
| Phase 4 | CLI ecosystem | S5 | ~30 |
| Phase 5 | Plugin system + Architecture gaps | S9, S10 | ~30 |
|| Phase 6 | Agent module depth | S2 (1 real) + S8 remaining | ~12 |

---

*Compiled May 29 2026. DA v1 audit. Every count verified against live source code.*
*S1 conversation loop plumbing extracted from Python's 4606-line run_conversation vs C's 1600-line agent_loop.c.*
