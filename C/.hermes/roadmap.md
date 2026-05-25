# ROADMAP — Hermes C Translation to 300 Gaps
**HONEST Plan: Close 136 gaps (436 → 300)**

## Strategy
1. ✅ Document all 436 gaps (battleship.md)
2. 🔴 Fix 67 F-N-F gaps (make code actually work)
3. 🟠 Implement 69 high-impact missing features
4. ⚪ Reach 300 total gaps (close 136)

---

## Phase 0: Fix Critical F-N-F (5 gaps → ship in 2 sessions)

| # | Gap | File | Fix | Effort |
|---|-----|------|-----|--------|
| 1 | Tool call loop broken | agent_loop.c | Move return to after TODO; implement multi-turn loop | 3h |
| 17 | Auth header malformed | llm_client.c | Fix `"...ype"` → `"Content-Type"` | 10min |
| 86 | web_search is alias | web.c | Implement real search (DuckDuckGo/SerpAPI) | 2h |
| 172 | Jobs memory-only | scheduler.c | Add JSON file persistence for jobs | 3h |
| 299 | state.md false claims | state.md | Already done ✅ (HONEST rewrite) | — |

**Result after Phase 0:** 431 gaps (5 closed) + 4 code fixes

---

## Phase 1: Fix Remaining F-N-F (62 gaps)

### Agent Loop (8 gaps)
| # | Gap | Fix |
|---|-----|-----|
| 2 | Tool result injection missing | Add `message_new_tool()` call |
| 3 | No tool call JSON parsing | Parse tool_calls array from LLM response |
| 4 | No iteration enforcement | Wire max_iterations into loop check |
| 5 | No interrupt handling | Add SIGINT handler |
| 6 | No error recovery | Add retry loop on LLM failure |
| 7 | No context window tracking | Add token counting estimate |
| 8 | Short-circuit on tool calls | Remove premature return |

### CLI (6 gaps)
| # | Gap | Fix |
|---|-----|-----|
| 39 | No readline | Integrate linenoise/readline |
| 40 | No autocomplete | Add tab completion for commands |
| 41 | No multi-line input | Detect and handle multi-line paste |
| 42 | No resize handling | SIGWINCH handler |
| 43 | No pager | Pipe long output through `less` |
| 84 | No `-q` flag | Proper getopt flag parsing |

### Tools (10 gaps)
| # | Gap | Fix |
|---|-----|-----|
| 87 | skills only list | Add skills_load, skills_view |
| 88 | No patch tool | Add find-replace patch |
| 89 | No search_files | Add grep/ripgrep wrapper |
| 90 | No PTY support | Add forkpty-based PTY |
| 91 | No background mode | Add process session tracking |
| 94 | Only 4 tools | Register 5 more tools each session |

### Gateway (6 gaps)
| # | Gap | Fix |
|---|-----|-----|
| 134 | No platform abstraction | Create adapter interface |
| 135 | No command approval | Add approval queue |
| 136 | No rate limiting | Add per-user rate counter |
| 137 | No multi-session | Per-chat agent context |

### Cron (3 gaps)
| # | Gap | Fix |
|---|-----|-----|
| 173 | "[]" stub | Implement real listing |
| 174 | No pause/resume | Add CLI integration |
| 175 | No output capture | Capture stdout/stderr |

### Config (6 gaps)
| # | Gap | Fix |
|---|-----|-----|
| 185 | Only 2 env vars read | Parse all 30+ provider keys |
| 186 | No hot reload | Add SIGHUP handler |
| 187 | No validation | Add schema check at load |
| 189-190 | No path/check commands | Add `hermes config path/check` |

### Display (5 gaps)
| # | Gap | Fix |
|---|-----|-----|
| 209-211 | Spinner/progress/panel missing | Implement declared functions |
| 212-213 | Color/width detection | Check TERM, query terminal size |

**Result after Phase 1:** ~369 gaps (67 F-N-F closed)

---

## Phase 2: Implement High-Impact Missing Features (69 gaps)

### Agent Loop (selected):
- Memory system (file-based key-value store)
- Context compression (summarize old turns)
- Session persistence (wire db.c into agent_loop)
- Session search (FTS5 via sqlite3 or grep index)

### CLI (selected):
- Top 10 most-used slash commands: /retry, /undo, /title, /compress, /config, /model, /tools, /skills, /reload, /history
- One-shot mode with `-m`, `-p`, `-q` flags

### Tools (selected):
- patch, search_files, vision_analyze, session_search, memory, clarify, todo, send_message, delegate_task

### Gateway (selected):
- Discord adapter (most requested after Telegram)
- Platform abstraction layer
- Webhook server

### Testing (selected):
- Test for each existing tool
- CI config for GitHub Actions
- `make test` target that actually runs tests

### Infrastructure (selected):
- CMakeLists.txt (portable build)
- `.clang-format`
- `.github/workflows/c-build.yml`
- `make install` target

**Result after Phase 2:** ~300 gaps (69 more closed)

---

## Phase 3: Polish & Extend (below 300)

Once at 300 gaps, the focus shifts:
1. Remaining 28 slash commands
2. Remaining 25 tools
3. Remaining 16 platforms
4. Provider support (Anthropic, Google, DeepSeek, xAI)
5. Profile system
6. Plugin system
7. MCP server
8. Full test coverage
9. Static compilation (musl)
10. Nix flake
11. WASM build

---

## Progress Tracker

| Date | Gaps | F-N-F | Missing | Stubs | Notes |
|------|------|-------|---------|-------|-------|
| May 25 | 436 | 67 | 362 | 1 | Initial audit |
| Target 1 | 400 | 40 | 353 | 1 | Fix critical F-N-F |
| Target 2 | 350 | 20 | 323 | 1 | Fix all F-N-F |
| Target 3 | 300 | 5 | 288 | 1 | Add high-impact features |

## Progress until these targets are hit:
- [ ] Phase 0: 431 gaps (5 fixed)
- [ ] Phase 1: ~369 gaps (67 F-N-F fixed)
- [ ] Phase 2: ~300 gaps (69 features added)