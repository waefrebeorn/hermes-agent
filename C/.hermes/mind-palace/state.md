# State — Hermes C Translation (2026-05-22)

## ~53% toward 1:1 Python parity (~390 gaps remaining)

### Milestone Dashboard

| Category | Gaps | Done% | Key Stats |
|----------|------|-------|-----------|
| **Config** | 6 depth | 95% | 322/322 YAML keys parsed, validated, env-overridden |
| **Providers** | 40 | 85% | 9 ops + 31 aliases + **18/18 LLM params** fully wired |
| **MCP** | 17 | **100% ✅** | Transport, tools, resources, prompts, subs, sampling, serve |
| **Plugins** | 51 | 8% | 3 .so stubs vs 45 Python backends (biggest structural gap) |
| **Gateway** | 63 | **~98% (62/63)** | E01-E53, E55-E63 ✅. E54 (Slack upload) blocked — needs HTTP multipart. E51 Python-only, skip |
| **Tools** | 24 | 95% | 28 reg'd, browser/memory/kanban 1:1. 6 CDP/plugin-blocked stubs |
| **Agent** | 32 | 85% | 23 state fields, 18 session DB, G01-G36 all filled |
| **CLI** | 34 | 85% | 70 slash commands, skin/theme engine. H31-H32 /session-search + /session-export added |
| **Libs** | 14 | 20% | libhttp/libcrypto/libcron ported |
| **Stdlib** | 5 | 30% | libproc/libcrypto basics |
| **Errors** | 5 | **100% ✅** | K01-K05: ValueError, TypeError, RuntimeError, OSError, TimeoutError |
| **Upstream** | 12 | new | 125 commits behind origin/main |
| **Tests** | 53 | 40% | 26 files, 1422 assertions |
| **Cross-cut** | 5 | **100% (5/5) ✅** | N02 secure parent dir, N05 local trust, N03 key leakage prevention, N04 vendor key derivation. Only N01 token counting remains as known gap |
| **Build/doc** | 15 | 30% | Cross-compile, Windows, Docker, CI |

### Session 2026-05-22 (Last Continuation)

- ✅ **K01-K05: Typed error hierarchy** — hermes_error.h/c with 5 error types, thread-local last error, format helpers. 11 tests, all pass
- ✅ **C04-C05: MCP sampling protocol** — incoming request queue, sampling callback, createMessage/respond/notify
- ✅ **C11-C13: MCP serve HTTP server** — POST /mcp endpoint, tool discovery via tools/list, tool call via tools/call, initialize/ping, CORS, lifecycle thread
- ✅ **E48: Discord interaction depth** — interaction type/id/token helpers, defer_interaction, show_modal
- ✅ **E52: Discord typing 429** — graceful per-channel cooldown tracker, 429 detection + backoff
- ✅ **E55: Matrix room management** — create_room, join_room, leave_room
- ✅ **N02: Secure parent dir** — chmod 0700 on config dir at load time, skip root
- ✅ **N05: Local provider trust** — detect localhost/127.0.0.1 in base_url, set local_provider flag
- ✅ **N03: API key leakage prevention** — provider_url_is_trusted() checks URL host matches provider's authoritative endpoint before sending API key in headers. Wired into llm_client.c (4 paths: provider+legacy, stream+non-stream)
- ✅ **N04: Vendor API key derivation** — provider_derive_api_key_name() derives <VENDOR>_API_KEY from base_url hostname. Hooks in config.c (both YAML and env-only paths)
- ✅ **H31: /session-search CLI command** — search past sessions, calls session_search_handler, displays ranked results with snippet
- ✅ **H32: /session-export CLI command** — export session as JSON or Markdown via session_crud_handler
- Commits: 7dd20224e, 38ae186c3, 5a8150ca1, b3a3ea0cf, eae17bdcf, 1c3bc1018, 1bfc8d03f, 4dabedce2
