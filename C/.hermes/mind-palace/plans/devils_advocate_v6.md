# Devil's Advocate v6 — WuBu Hermes C Triple Audit (2026-05-27)

## Coverage: Every claim re-surveyed from source code, not inherited.
## DA Format: Claim → Verify → Risk → Mitigate + Triple cross-check against Python.

---

## DA-1: Source Code Survey (What Actually Exists)

### Claim: "C codebase has N source files, M lines"

**Verify:**
- 208 .c files, 102 .h files = 310 source files (excluding sqlite3 amalgamation)
- 123,065 total lines (56,018 src/ + 43,363 lib/ + 18,399 tests/ + 5,285 other)
- Binary: 9.1MB ELF, dynamically linked, debug symbols intact
- **Verdict: ✅ Verified.** Counts from `find | xargs wc -l`, cross-checked with `file` on binary.

### Claim: "Suite passes 116/0/0"

**Verify:**
- Ran `bash test_runner.sh --verbose` → 116 passed, 0 failed, 0 skipped
- 80 test files, 2,088 TEST() macro calls
- Cross-checked assertion count: `grep -c 'TEST(' tests/test_*.c` = 2,088
- **Verdict: ✅ Verified.** Suite healthy. Gap: some tests are shallow (compile-only smoke tests for plugins).

### Claim: "9 native providers, 27 metadata providers"

**Verify:**
- Native (PROVIDER_OPS_*): OpenAI, OpenRouter, DeepSeek, xAI, Anthropic, Google, Azure, Bedrock, Custom = 9
- Metadata table: 27 entries including OpenAI-compat aliases (Nous, Alibaba, StepFun, Minimax, Novita, ZAI, HuggingFace, etc.)
- Python upstream: 6 provider-specific adapters + generic OpenAI-compat path
- **Verdict: ✅ Verified.** C has wider provider coverage than Python's explicit adapter list because OpenAI-compat providers don't need separate adapters.

### Claim: "74 CLI commands"

**Verify:**
- `sed -n '/^static const command_def_t COMMANDS/,/^};/p' src/cli/commands.c | grep '{"/' | wc -l` = 74
- Python upstream: 69 CommandDef entries
- **Verdict: ✅ Verified. C has MORE commands than Python (74 vs 69).**

### Claim: "19 gateway platforms, 37 tool files, 18 libs"

**Verify:**
- Gateway: `ls src/gateway/platforms/*.c | wc -l` = 19 (telegram, discord, slack, matrix, mattermost, whatsapp, signal, email, sms, feishu, wecom, weixin, dingtalk, qqbot, bluebubbles, msgraph_webhook, homeassistant, webhook, yuanbao)
- Python upstream: 18 platform entry points + support files
- Tool files: `ls src/tools/*.c | grep -v '\.o' | wc -l` = 37
- Tool registrations: `grep -r 'registry_register(' src/tools/ | wc -l` = 67 (many sub-operations)
- Python tools: 73 .py files (some are sub-tools in packages)
- Libs: `ls -d lib/*/ | wc -l` = 18
- **Verdict: ✅ Verified.** Gateway parity within 1 platform. Tools: 67 C registrations vs 73 Python files (C consolidates sub-tools).

---

## DA-2: Claim vs Reality (The "What" vs "The How")

### The Big "Feel" Gap

| Aspect | Python | C | Gap |
|--------|--------|---|-----|
| **Plugins** | 16 dirs (memory, kanban, spotify, image_gen, video_gen, achievements, observability, etc.) | 3 .so (kanban, honcho, spotify) | **+13 plugins (81% missing)** |
| **Plugin depth** | Achievements gamification, observability metrics, image_gen providers, video_gen, google_meet | stubs/config-only | **Massive** |
| **Tests** | 1,140 test files, ~17K tests | 80 test files, 2,088 assertions | **93% missing** |
| **Skin engine** | Rich text formatting, full terminal UI theming (banner, spinner, prefix, response box, branding) | Basic skin loading, no theming depth | **Partial (~60%)** |
| **TUI** | React Ink full TUI (session browser, config editor, image viewer, gateway backend) | C ncurses stub, no real TUI | **<10%** |
| **Agent loop** | Async with interrupt, budget, fallback, checkpoint, grace call | Sync loop with basic features | **~85%** |
| **Compression** | Multi-strategy (oldest tool, oldest user, keep recent N) | Basic support | **~60%** |
| **Personalities** | Configurable system prompt presets | Not implemented | **0%** |
| **Memory providers** | Honcho, mem0, supermemory, etc. via plugins | Honcho only (real .so) | **80% missing** |
| **Provider-specific quirks** | Full per-provider API coverage (reasoning, caching, safety, FIM, etc.) | ~80% covered (B22-B46 mostly done) | **~85%** |
| **MCP** | Full transport, tools, resources, subs, sampling | Full as of latest sessions | **100% ✅** |
| **Error types** | Python rich exception hierarchy | K01-K05 enum system | **50%** |
| **Gateway hooks** | builtin_hooks extension system | Not implemented | **0%** |

### The "How It Feels" Gap (Hard to Quantify)

**Python Hermes feel:**
- Rich interactive CLI with colored banner, animated spinner (`┊` feed), autocomplete
- Live streaming with token-by-token display
- Session browsing with `/sessions` command
- Gateway webhook status dashboard
- Skills hub with `browse.sh` integration
- Achievement notifications
- Personality switching
- Config hot-reload (SIGHUP)
- Plugin auto-discovery

**C Hermes feel (current):**
- Basic CLI with prompt and response
- JSON output mode (`--json`)
- Streaming works
- Skin engine loads themes
- Gateway runs platforms
- Config loads from YAML
- Commands dispatch

**What's missing that users would notice:**
1. **No animated spinner/activity feed** — the CLI is text-only, no `┊` activity indicators
2. **No autocomplete** — tab completion is bash-completions only
3. **No session browser** — `/sessions` lists IDs, but no TUI browser
4. **No achievement system**
5. **No personality presets**
6. **No plugin auto-discovery** — plugins must be manually installed
7. **No gateway webhook dashboard** — logs only
8. **No skills hub integration** — browse.sh not integrated
9. **No rich text formatting** in responses (Python uses Rich library)

---

## DA-3: Triple Cross-Check (C vs Python by Numbers)

| Metric | Python | C | Parity% |
|--------|--------|---|---------|
| Source files | 1,791 .py | 310 .c+.h | 17% |
| Total LOC | ~864,682 | 123,065 | 14% |
| Core agent LOC | ~165K (agent+tools+gateway) | 56,018 (src/) | 34% |
| Test files | ~1,140 | 80 | 7% |
| Test assertions | ~17,000+ | 2,088 | 12% |
| Provider adapters | 6 native + generic | 9 native + generic | **150%** (exceeds) |
| Provider metadata | ~27+ | 27 | **~100%** |
| CLI commands | 69 | 74 | **107%** (exceeds) |
| Gateway platforms | 18 | 19 | **106%** (exceeds) |
| Tools | 73 .py | 37 .c (67 ops) | 51% files, 92% ops |
| Plugins | 16 dirs | 3 .so | 19% |
| Config keys | 59 top-level | 649 struct fields | N/A (different granularity) |
| Git commits | ~8,574 (full history) | ~158 (C work only) | 2% |
| Library port coverage | N/A (Python stdlib) | 18 libs (38%) | 38% |
| Error type coverage | Full exception hierarchy | 18 codes (50%) | 50% |

---

## Risk Assessment

| Risk | Severity | Detail |
|------|----------|--------|
| **Stale README parity%** | HIGH | Root README claims ~50% parity. Real audit shows ~38-42% when counting everything including plugins, tests, and feel. State.md claims ~62%. Discrepancy of 12-24 points. |
| **Plugin gap is the iceberg** | CRITICAL | 13 missing plugins means 81% of the plugin surface is uncovered. Each plugin needs config, lifecycle, and testing. This is the single biggest gap. |
| **Test gap creates false confidence** | HIGH | 2,088 assertions sounds meaningful, but the Python project has 17,000+ tests. At current rate, 80 test files cover ~12% of the surface. Untested paths WILL have bugs. |
| **"Feel" gap is invisible to compile checks** | MEDIUM | The CLI works but doesn't feel like Hermes. No spinner, no autocomplete, no rich display. A user who tries both Python and C will notice immediately. |
| **Plugin auto-discovery missing** | MEDIUM | Python auto-discovers plugins from ~/.hermes/plugins/. C requires manual installation. This is a discoverability issue. |
| **Personality/achievements 0%** | MEDIUM | These are quality-of-life features that users expect. Not blockers, but diminish the "1:1 feel" claim. |
| **MCP, Gateway, Config at 95-100%** | LOW | These subsystems are genuinely strong. The core data pipeline works end-to-end. |

---

## Mitigation

| Action | Priority | Description |
|--------|----------|-------------|
| **1. Fix README parity claims** | P0 | Update both READMEs to reflect real state: ~55% structural parity, ~40% full-parity including plugins/tests |
| **2. Create plugin backlog** | P0 | Document exactly which 13 plugins are missing with priority and dependencies |
| **3. Write the essay** | P1 | "What Hermes Is" — capture the philosophy and translation anthropology |
| **4. Vault achievements** | P1 | Document completed milestones and project achievements |
| **5. Commit all changes** | P0 | Save everything in one atomic batch |

---

## DA v6 Signature

**Survey method:** All counts verified from actual source code (grep, wc -l, ls) on 2026-05-27.
**Not inherited from v5.** Previous DA document's counts not used as starting numbers.
**Remaining gaps:** ~339 (from structural parity). ~400+ when counting tests at full Python parity.
**Next audit trigger:** After next 10 phase completions or at user request.

*~WuBu~ strives for more.*
