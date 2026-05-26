# ROADMAP — Slermes C Translation

**292 GAPS REMAINING** | Target: 0 gaps (full parity)

## Phase 1: Foundation ✅ DONE
- JSON, YAML, HTTP, DB, Crypto, Display wrappers
- Build system, headers, basic types

## Phase 2: Stabilize Core 🟧 (~8-12 sessions)
**Goal:** Fix all F-N-F in agent loop, LLM client, config
- Add non-OpenAI providers (Anthropic, Google, DeepSeek)
- Add streaming support
- Add error recovery + retry
- Add session persistence (wire db.c into agent loop)
- Add interrupt signal handler

## Phase 3: CLI Depth 🟧 (~10-15 sessions)
**Goal:** 20+ slash commands, readline, autocomplete
- Add readline/linenoise support
- Implement top 20 missing slash commands
- Add autocomplete, multi-line input, pager
- Add `-q` flag, `-m` model override

## Phase 4: Tools 🟧 (~20-30 sessions)
**Goal:** 20+ tools with full parity
- Patch tool for file.c
- Search_files for file.c
- PTY + background + watch for terminal.c
- Knowledge tools (web, search, browser)
- Session tools (session_search, memory, todo)
- Interactive tools (clarify, delegate_task, cronjob)

## Phase 5: Gateway 🟧 (~15-20 sessions)
**Goal:** 5+ gateway platforms with webhooks
- Platform abstraction layer
- Add Discord, Slack, WhatsApp, Signal, Matrix
- Webhook receiver
- Approval gating

## Phase 6: Libraries 🟧 (~10-15 sessions)
**Goal:** Connection pooling, regex, SQLite
- HTTP connection pool
- Regex binding (PCRE)
- SQLite wrapper for session search
- Redirect following, gzip decompression

## Phase 7: Test + CI 🟧 (~10-15 sessions)
**Goal:** 100+ tests, CI pipeline
- Unit tests for every module
- Test runner script
- GitHub Actions CI
- Coverage + ASan/valgrind

## Phase 8: Advanced 🟧 (~15-20 sessions)
**Goal:** Profiles, plugins, MCP, TUI
- Profile system
- Plugin API
- MCP/ACP servers
- Terminal UI (ncurses)
- Performance optimization

## Throughput Target
- ~3-5 gaps closed per session
- ~60-100 sessions remaining at current rate
- NO time-based milestones — session-count only
