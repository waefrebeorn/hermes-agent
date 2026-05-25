# Changelog

## [Unreleased] — 2026-05-31

### Added
- **Triple DA v8**: 313 verified gaps across 22 sectors — stub hunt, placeholder scan, Python-vs-C module comparison
- **battleship-v8.md**: 313 verified gaps (9 P1, 193 P2, 111 P3) — all stale claims from v7 retired
- **Vault Phase 8**: 16 stale claims retired with evidence table
- **~user tilde expansion**: getpwnam-based path resolution
- **Message-level DB queries**: db_query_tool_stats() + tool breakdown in /insights
- **--source filter**: per-platform insights filtering
- **Glob pattern allowlist**: approval.c supports glob patterns instead of substring
- **X11/Wayland set_value**: type-based fallback for computer_use

### Fixed
- B01: Buffer overflow in meta_to_json (insights crash)
- R01/W01: llm_background_review wired to /curator run
- S03/S04: x11_set_value/wayland_set_value replaced with real fallbacks
- Skills use-after-free in skills_list_handler
- xAI reasoning_effort from config
- NO_PROXY env var bypass
- patch.c fuzzy match offset 0 buffer overflow guard

### Suite
- 237/0/0 — 202 test files, zero failures

## [v0.14.1] — 2026-05-23

### Added
- J18 libwebsocket: 14 tests, .a target
- J19 libtoml: minimal TOML v1.0 parser, 25 tests
- J20 libjson5: JSON5 preprocessor, 60 tests
- J22 libansi: ANSI terminal codes, 18 tests
- P54 tool_config test: 25 assertions for runtime overrides/env resolution
- P21 CLI paths test: 15 assertions for path resolution/profile management

### Suite
- 152/0/0

## [v0.14.0] — 2026-05-22

### Added
- Provider test suite: Anthropic (80), Google (40), Bedrock (35), Azure (45)
- Provider error test (M06): 225 assertions across 9 providers
- Config env priority tests (M23): 10 assertions
- Config validation edge cases (M22): 10 assertions
- finish_reason tracking (B22): 20 change sites across 10 files
- json_mode convenience flag (B23): auto-sets response_format
- Google provider depth (B30-B31): top_k, candidate_count, systemInstruction
- Vault encryption test (O11): 37 assertions
- Audit log rotation (O12): 11 assertions
- File permission hardening (O15): 15 assertions
- Sandbox escape detection (O14): 48 patterns
- TIRITH policy engine (O13): 4 rule types, 15 built-ins

### Fixed
- temperature=0.0 silent drop — all 9 providers
- response_format use-after-free — all 9 providers
- NULL stream chunk SIGSEGV — 6 providers
- API error JSON silently dropped — 6 providers
- Redact heap overflow
- Google tools functionDeclarations no-op
- x_search auth header literal `***` bug
- Google/Anthropic/Bedrock NULL stream crash

### Suite
- 96/0/0 → 117/0/0

## [v0.12.0] — 2026-05-22

### Added
- A03: `${VAR:-default}` env expansion in config
- A04: `!include` directive for YAML config
- J04-J12: libpath (76), libdatetime (59), libcsv, libhash (25), libuuid (60), libbase64 (34), libhtml, libtextwrap (35), libglob (21), libsignal (9), libenum (22), libdifflib (23), libregex (21)
- J13-J16: libregex, libjson5, libwebsocket (14), libtoml (25), libansi (18)
- K06-K20: Complete error type hierarchy (58 error codes)
- Docker, CI, cross-compile, man page
- Spotify plugin (real Web API), Kanban plugin (real in-memory board)

### Suite
- 88/0/0

## [v0.11.0] — 2026-05-22

### Added
- MCP SSE transport (G55)
- Agent loop retry + fallback (Phase 113)
- File tool test (M31): 35 assertions
- Web tool test (M30): 22 assertions
- BSM Secrets Manager (L01)
- xAI encrypted reasoning replay (L07)
- xAI Web Search (L03)

### Suite
- 82/0/1

## [v0.10.0] — 2026-05-20
- Phase 5: Full cron scheduler
- Phase 4: Gateway (Telegram long-polling)
- Phase 3: Tools (registry, terminal, file, web, skills)
- Phase 2: Agent core (loop, LLM client, CLI, config)
- Phase 1: Foundation deps (JSON/YAML/HTTP/DB/crypto/display)

### Suite
- 59/0/3
