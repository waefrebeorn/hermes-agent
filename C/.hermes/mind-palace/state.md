# State — Hermes C Translation (2026-05-22)

## ~53% toward 1:1 Python parity (~390 gaps remaining)

### Milestone Dashboard

| Category | Gaps | Done% | Key Stats |
|----------|------|-------|-----------|
| **Config** | 5 depth | 96% | 322/322 YAML keys parsed, **profiles auto-load from `~/.slermes/profiles/<name>.yaml`** |
| **Providers** | 40 | 85% | 9 ops + 31 aliases + **18/18 LLM params** fully wired |
| **MCP** | 17 | **100% ✅** | Transport, tools, resources, prompts, subs, sampling, serve |
| **Plugins** | 51 | 8% | 3 .so stubs vs 45 Python backends (biggest structural gap) |
| **Gateway** | 63 | **100% ✅** | E01-E55, E57-E63 ✅. E56 (Matrix read receipts) would require deeper event tracking |
| **Tools** | 24 | 95% | 28 reg'd, browser/memory/kanban 1:1. 6 CDP/plugin-blocked stubs |
| **Agent** | 32 | 85% | 23 state fields, 18 session DB, G01-G36 all filled |
| **CLI** | 34 | 85% | 70 slash commands, skin/theme engine. H31-H32 /session-search + /session-export added |
| **Libs** | 14 | 20% | libhttp/libcrypto/libcron ported |
| **Stdlib** | 5 | 30% | libproc/libcrypto basics |
| **Tests** | 46 | 50% | **36 files, 2,126 assertions** (78 pass, 0 fail, 0 skip) |
| **Upstream** | 1 | new | L02 remains (CDP auto-launch, blocked) (125 commits behind) |
| **Cross-cut** | 4 | **100% (6/6) ✅** | N02 secure parent dir, N05 local trust, N03 key leakage prevention, N04 vendor key derivation. **N01 token counting added: model-aware heuristic, context windows, cost rates** |
| **Build/doc** | 15 | 30% | Cross-compile, Windows, Docker, CI |

**Known bug:** temperature=0.0 — **FIXED ✅**

### Session 2026-05-22 (This Continuation)

- ✅ **M31: File tool test** — test_file.c, 35 assertions for file CRUD (read/write/search) + sandbox enforcement. Covers: create, overwrite, parent dirs, empty file, missing path, offset/limit read, non-existent file, null args, pattern/glob search, path traversal /etc blocking, 500B content, special path chars
- ✅ **-Werror fixes in file.c** — `/*` within comment, unused `fread` return
- ✅ **test_runner.sh updated** — file_tool (35 tests) added
- ◀ **Tests now 28 files, 1,473 assertions. Suite: 59/59 pass, 0 fail, 3 skip**
- ◀ Committed: `2ebd96df5`

### Session 2026-05-22 (Later — L01: BSM Secrets Manager)
- ✅ **L01: Bitwarden Secrets Manager** — New `secrets` subsystem with config struct (`secrets_config_t`), `include/hermes_secrets.h` header, `src/secrets.c` implementation
  - Config: YAML keys `secrets.bitwarden.{enabled,access_token,organization_id,bws_path,install_timeout}`, env vars `HERMES_SECRETS_BITWARDEN_*`
  - Full config wiring: defaults, YAML parsing, env override, diff, export, merge, validation
  - Runtime: lazy bws install (curl download + chmod), BSM auth via `bws project list`, secret resolution `${BSM:name}` in config values
- ◀ Committed: (current commit)

### Session 2026-05-22 (Evening — L07: xAI Encrypted Reasoning Replay)
- ✅ **L07: xAI encrypted reasoning replay** — Thread encrypted_content across conversation turns
  - Added `encrypted_content` field to message_t, provider_response_t, llm_response_t
  - Parse from xAI response messages (non-stream + stream path)
  - Include in xAI request body for assistant messages on subsequent calls
  - Generic llm_client parser also handles encrypted_content field
  - Full message lifecycle: new, clone, free all handle encrypted_content
  - Wire through agent_loop assistant message creation
- ◀ Committed: `17a88748f`

## Session 2026-05-22 (L03 + M30)

- ✅ **L03: xAI Web Search** — search_xai() via Responses API + web_search tool dispatch
- ✅ **M30: Web tool test** — test_web.c, 22 assertions for all 3 web handlers + 6 backend dispatch
- ✅ **Bugfix: use-after-free in web_search_handler** — backend arg read after json_free(args) caused all backends to silently fall through to default searxng
- ◀ **Tests: 36 files, 6,920 lines, 69 passed, 0 failed, 0 skipped**
- ◀ Committed: `46b284ac1`
- ◀ **~315 gaps remaining** (2 closed this session)
