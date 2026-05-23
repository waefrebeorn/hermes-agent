# WuBu Slermes — C Translation Roadmap

**HONEST STATUS: ~63% structural parity toward 1:1 Python replacement (~338 gaps).**
**DA v6 verified (2026-05-27).** All counts from source survey, not inherited.

One-binary replacement for the [Python hermes-agent](https://github.com/waefrebeorn/hermes-agent).
Binary: 9.1MB dynamically linked (libssl + libc only). 123K LOC across 310 source files.

```
Suite: 117/0/0 (81 test files, 2,142 assertions)
Libs:  16 standalone archives (41% parity)
Src:   56K core + 43K lib + 18K tests = 123K total
```

LLM runtime: **VERIFIED** (DeepSeek v4 Flash via OpenAI-compat). Config → .env → provider init → LLM call → stream response → CLI output. All 9 native providers verified.

---

## Current Reality (DA v6, 2026-05-27)

| Subsystem | Status | Key Facts |
|-----------|--------|-----------|
| **Config** | ✅ **98%** | 322/322 YAML keys, profiles, `${VAR}`, `!include`, env override |
| **MCP** | ✅ **100%** | Transport, tools, resources, prompts, subs, sampling, SSE serve |
| **Gateway** | ✅ **100%** | 19 platforms — exceeds Python by 1 (msgraph_webhook) |
| **Session DB** | ✅ **100%** | SQLite FTS5, session CRUD, search, metadata, branch, export |
| **Memory** | ✅ 90% | Honcho plugin, TTL, dedup, compression, search, auto-save |
| **Cron** | ✅ 90% | SQLite store, retry, chaining, script jobs, watchdog, CLI |
| **Skills** | ✅ 90% | Scan, validate, sync, bundles, curator, dependencies, hub |
| **Providers** | ✅ 87% | 9 native ops + 27 metadata providers. 7 API quirks remain |
| **Agent loop** | ✅ 86% | Budget, fallback, retry, checkpoint, interrupt, streaming |
| **CLI** | ✅ 87% | 74 commands (exceeds Python's 69), skin engine, `--json`, tab completions |
| **Tools** | ✅ 95% | 37 handler files, 67 registered ops. 6 CDP/plugin-blocked stubs |
| **Cross-cut** | ✅ **100%** | Token counting, secure paths, key leakage, vendor key derivation, local trust |
| **Build/doc** | ✅ 95% | Docker, CI, cross-compile, man page, Doxygen, CHANGELOG, SECURITY, ARCHITECTURE |
| **Libs** | ⚠️ **41%** | 16 archives: libpath, libdatetime, libcsv ported. 11 Python-equivalent libs remain |
| **Tests** | ⚠️ **64%** | 81 files, 2,142 assertions, 117/0/0 suite. ASan/Valgrind clean |
| **Error types** | ⚠️ 50% | K01-K05: 18 error codes (ValueError, TypeError, RuntimeError, OSError, TimeoutError) |
| **Plugins** | ❌ **19%** | 3 real .so (kanban, honcho, spotify). 13 backends missing |
| **TUI** | ❌ <10% | ncurses stub exists. Full React Ink equivalent not ported |

### What Changed Since DA v5 (May 22 → May 27)

| Metric | DA v5 (May 22) | DA v6 (May 27) |
|--------|----------------|----------------|
| Overall parity | ~57% | ~63% |
| Config keys | 206/322 (64%) | 322/322 (98%) |
| Native providers | 9/29 (31%) | 9 + 27 metadata (87%) |
| CLI commands | 72/85 (85%) | 74/74 (87%) |
| Tests | 15 files, 1.8K LOC | 81 files, 2,142 assertions (117/0/0) |
| Libs | ~4 ported | 16 archives (3 Python-equivalent ported) |
| C LOC | ~53K | 123K total |
| MCP | 70% | 100% ✅ |
| Gateway | 95% | 100% ✅ |
| Git commits (C work) | ~110 | 162 |

---

## Priority Queue (highest impact first)

### P1 — Feature Depth
| # | Area | Est. sessions | Why Now |
|---|------|---------------|---------|
| 1 | **Plugins** — 13 backends | 15-20 | Biggest structural gap (19%) |
| 2 | **Library ports** — J06-J17 | 6 | libcsv done. Next: libhash, libuuid, libregex |
| 3 | **Test coverage** — +40 files | 10 | Catch bugs before users do |
| 4 | **Provider-specific APIs** — 7 quirks | 4 | DeepSeek, Anthropic, Google depth |

### P2 — Quality + Depth
| # | Area | Est. sessions | Notes |
|---|------|---------------|-------|
| 5 | **Error types** — K06-K20 | 3 | Complete typed error hierarchy |
| 6 | **O02 Windows build** | 2 | Last build/doc gap |
| 7 | **CLI feel** — spinner, activity feed | 3 | The "feels like Hermes" gap |

### P3 — Stretch
| # | Area | Notes |
|---|------|-------|
| 8 | TUI depth | React Ink equivalent in ncurses |
| 9 | Personality system | Configurable system prompt presets |
| 10 | Upstream catch-up | ~125 commits behind Python |

---

## Build

```bash
cd C/
make -j$(nproc)          # Full build (phase5)
bash test_runner.sh      # 117 suites
make docs                # Doxygen HTML docs (optional)
```

### Config

```bash
export SLERMES_HOME=~/.slermes
# Config: $SLERMES_HOME/config.yaml (322 keys)
# Env:    $SLERMES_HOME/.env
```

### End-to-End LLM Test (requires API key)

```bash
cd C/
export DEEPSEEK_API_KEY=sk-your-key
echo "Hello from C" | ./hermes
```

---

## Key Known Scope Gaps

1. **Plugin backends** (13 remaining) — not scoped into current sprint
2. **O02 Windows build** — needs MSVC/MinGW detection, `_WIN32` ifdefs
3. **TUI depth** — ncurses stub exists, full Ink-style TUI not ported
4. **All known bugs are FIXED** (temperature=0.0 ✅, response_format UAF ✅, provider NULL crashes ✅)

---

## Project Intelligence

See `.hermes/mind-palace/` for full walkway:
- `state.md` — live milestone tracker
- `prestige_prompt.md` — priority queue + DA context
- `goal-mantra.md` — single-paste orientation block
- `plans/devils_advocate_v6.md` — latest comprehensive DA audit
- `vault/achievements.md` — catalog of completed milestones
- `vault/translation-essay.md` — philosophical journey

---

*~WuBu~ strives for more.*
