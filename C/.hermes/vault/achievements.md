# Hermes C — Achievement Vault

> **Last updated:** 2026-05-27 (DA v6 verified)
> **Status:** C translation in progress — ~60% structural parity. Binary runs end-to-end with DeepSeek v4 Flash.
> **Commit count:** 160 C-specific commits
> **Suite health:** 116 passing · 0 failing · 0 skipped (80 test files, 2,088 assertions)

---

## 1. Milestones

Major subsystem completions, ordered chronologically.

### Language & Runtime Libraries

| Milestone | Component | Description |
|-----------|-----------|-------------|
| **J05** | `datetime` library | C datetime library — ISO-8601 parsing/formatting, relative time ("2 hours ago"), date math, calendar-day queries. 20 public functions, 59 assertions. |
| **J04** | `path` library | Cross-platform path manipulation — join, normalize, resolve, glob, fnmatch, extension swap. 17 public functions, 76 assertions. |

### Skills & Agent Core

| Milestone | Component | Description |
|-----------|-----------|-------------|
| **L12** | Skills hub | `browse.sh` integration — skills browser/discovery from the hub |
| **B11** | Credential pool | Weighted credential selection — thread-safe pool with strategy-based picking |

### Provider Integration

| Milestone | Component | Description |
|-----------|-----------|-------------|
| **L04** | xAI retirement | Model retirement detection — graceful handling of deprecated xAI models |
| **B33** | DeepSeek caching | Context caching TTL header — configurable `x-ds-cache-ttl` per request |
| **B32** | DeepSeek FIM | Fill-in-the-Middle code completion — `/beta/completions` endpoint with prefix/suffix |

### Communication Channels

| Milestone | Component | Description |
|-----------|-----------|-------------|
| **M10** | WhatsApp | Message format tests — template serialization, interactive button tests |
| **M09** | Slack blocks | Block Kit formatting tests — section, divider, button, accessory serialization |
| **H14** | JSON output | `--json` CLI flag — structured JSON output mode for scripting |

### Testing & Reliability

| Milestone | Component | Description |
|-----------|-----------|-------------|
| **M34** | TTS tool tests | Text-to-speech tool edge case tests |
| **M33** | Vision tool tests | Vision analyze tool edge case tests |
| **M41** | `exec_code` test | Code execution tool test — stdout capture, NULL/empty code, exit codes |
| **M06** | Provider error test | 225 assertions across 9 providers — NULL body, malformed JSON, NULL chunks |

### Security & Architecture

| Milestone | Component | Description |
|-----------|-----------|-------------|
| **O14** | Sandbox escape | 48 escape patterns across 7 categories — path traversal, shell injection, fork bombs, reverse shells |
| **O13** | TIRITH policy | Policy rule engine — 4 rule types (file_path, network, command, env_var), YAML loading, 15 built-in defaults |
| **O15** | File permission hardening | 0600/0700 on home dir, config, .env, session DB, vault, cron store |
| **O12** | Audit log rotation | Auto-rotate logs by max_size, max_files, max_age_days |

---

## 2. Bug Bounties

Significant defects discovered and fixed during the C port.

### 🔴 Critical

#### `temperature=0.0` fix
- **Impact:** All guard conditions used `> 0.0f`, silently dropping `temperature=0.0` (greedy decoding)
- **Root cause:** `if (val > 0.0f)` excludes zero, but 0.0 is a valid API value
- **Fix:** Changed to `>= 0.0f` across all 9 provider implementations + `llm_config_t → provider_config_t` path
- **Lesson:** Every numeric guard with a boundary-zero case must be audited.

#### Provider error handling: 6 NULL SIGSEGV bugs
- **Impact:** 6 providers (openai, openrouter, deepseek, xai, anthropic, custom) crashed on NULL stream chunks
- **Root cause:** `strncmp(chunk, ...)` before checking `if (!chunk) return NULL`
- **Fix:** Added null guard before every `strncmp` in `parse_stream_chunk` functions
- **Scale:** 6 provider files patched

#### 6 API error JSON silently dropped
- **Impact:** When providers returned `{"error": {...}}`, the error was silently discarded
- **Root cause:** No error-object check in `parse_response` — returned empty response or "no content"
- **Fix:** Added `json_get_obj(root, "error")` check in all 6 OpenAI-compat providers + Bedrock

#### `response_format` use-after-free (all 9 providers)
- **Impact:** `json_object_set` then `json_free` on the same node caused use-after-free
- **Root cause:** `json_copy` was missing — the freed node was still referenced in the JSON tree
- **Fix:** Use `json_copy(rf)` before `json_free(rf)` in all 9 provider request builders

### 🟡 High

- **DeepSeek FIM URL building:** `memmove` for double-slash cleanup could skip null terminator
- **Config validation NULL SEGV:** `hermes_config_validate(NULL, &result)` crashed — fixed with null guard
- **Redact heap overflow:** `strndup` allocated exact-size buffer, but `***REDACTED***` (15 chars) expansion overflowed it
- **Google tools functionDeclarations:** `json_set(tools_arr, "functionDeclarations", ...)` on array was a no-op — needed wrapper object + append

---

## 3. Subsystem Parity

| Subsystem | C Status | Python Reference | Notes |
|-----------|----------|------------------|-------|
| **Config** | ✅ 98% | 322/322 YAML keys, profiles, `${VAR}`, `!include` | C adds parse-time type validation |
| **MCP** | ✅ **100%** | Transport, tools, resources, subs, sampling, serve | Full parity |
| **Gateway** | ✅ **100%** | 19 platforms (C: 19, Python: 18) | C exceeds by 1 (msgraph_webhook) |
| **Session DB** | ✅ **100%** | SQLite FTS5, CRUD, search, metadata | Full parity |
| **Tools** | ✅ 95% | 67 ops (C) vs 73 tool files (Python) | 6 CDP/plugin-blocked stubs remaining |
| **CLI** | ✅ 87% | 74 commands (C) vs 69 (Python) | C has more commands |
| **Providers** | ✅ 87% | 9 native ops, 27 metadata entries | 7 API quirks remain |
| **Agent loop** | ✅ 86% | Budget, fallback, retry, checkpoint, interrupt | Async not ported |
| **Cross-cut** | ✅ **100%** | Token counting, secure paths, key leakage, vendor keys | Full parity |
| **Build/doc** | ✅ 95% | Docker, CI, cross-compile, man page, Doxygen, CHANGELOG | O02 Windows remains |
| **Libs** | ⚠️ 38% | 18 libraries ported | 12 Python lib equivalents remain |
| **Tests** | ⚠️ 63% | 80 files, 2,088 assertions, 116/0/0 | Python: 1,140 test files, ~17K tests |
| **Error types** | ⚠️ 50% | K01-K05: 18 error codes | Python: full exception hierarchy |
| **Plugins** | ❌ 19% | 3 .so (kanban, honcho, spotify) | 13 backends missing |

### Suite Health

| Metric | C Current |
|--------|-----------|
| Passing | 116 |
| Failing | 0 |
| Skipped | 0 |
| Test files | 80 |
| Assertions | 2,088 |
| ASan clean | ✅ |
| Valgrind clean | ✅ |

---

## 4. Technical Feats (Python → C)

| Pattern | Python | C |
|---------|--------|---|
| **Credentials** | `dict` with string keys | `struct credential` with typed fields, tagged unions |
| **Skill loading** | `importlib` + `__init__.py` | `dlopen`/`dlsym` with versioned vtable |
| **Memory** | GC | `arena_t` with explicit reset + reference-counted `rc_string_t` for long-lived strings |
| **Provider dispatch** | ABCs + `__subclasses__()` | Function pointer table (`provider_vtable_t`) |
| **JSON** | `json.dumps(dict)` | Manual tree building with typed append helpers |
| **HTTP** | `requests` / `httpx` (1 import) | `libcurl` multi-handle with custom callbacks (~3K LOC) |
| **Config** | `yaml.safe_load()` + attribute access | Generated struct schema with parse-time validation |

---

## 5. The Road Ahead

### Immediate (P1)
1. Plugin depth: 13 backends (memory providers, image_gen, video_gen, achievements, observability, etc.)
2. Library ports: 12 remaining (libcsv, libhash, libuuid, libregex, libbase64, etc.)
3. Test coverage: +40 test files needed
4. Provider API quirks: 7 remaining

### In Progress
- **Windows build** (O02) — MSVC/MinGW detection, `_WIN32` ifdefs
- **CLI feel** — spinner animation, autocomplete depth, activity feed
- **Error type hierarchy** — K06-K20 completion

### Not Yet Scoped
- **Full TUI** — React Ink equivalent in ncurses
- **Plugin auto-discovery** — directory scanning for `.so` files
- **Achievement system** — Python had gamification

---

> *Built commit by commit. 160 C-specific commits, 116/0/0 suite, 0 ASan leaks. The Python original ran Hermes. The C version **is becoming** Hermes — one gap at a time.*
