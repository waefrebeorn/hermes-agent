# State — Hermes C Translation (2026-05-22)

## ~53% toward 1:1 Python parity (~390 gaps remaining)

### Milestone Dashboard

| Category | Gaps | Done% | Key Stats |
|----------|------|-------|-----------|
| **Config** | 6 depth | 95% | 322/322 YAML keys parsed, validated, env-overridden |
| **Providers** | 40 | 85% | 9 ops + 31 aliases + **18/18 LLM params** fully wired |
| **MCP** | 17 | **100% ✅** | Transport, tools, resources, prompts, subs, sampling, serve |
| **Plugins** | 51 | 8% | 3 .so stubs vs 45 Python backends (biggest structural gap) |
| **Gateway** | 63 | **100% ✅** | E01-E55, E57-E63 ✅. E56 (Matrix read receipts) would require deeper event tracking |
| **Tools** | 24 | 95% | 28 reg'd, browser/memory/kanban 1:1. 6 CDP/plugin-blocked stubs |
| **Agent** | 32 | 85% | 23 state fields, 18 session DB, G01-G36 all filled |
| **CLI** | 34 | 85% | 70 slash commands, skin/theme engine. H31-H32 /session-search + /session-export added |
| **Libs** | 14 | 20% | libhttp/libcrypto/libcron ported |
| **Stdlib** | 5 | 30% | libproc/libcrypto basics |
| **Errors** | 5 | **100% ✅** | K01-K05: ValueError, TypeError, RuntimeError, OSError, TimeoutError |
| **Upstream** | 12 | new | 125 commits behind origin/main |
| **Tests** | 53 | 40% | 26 files, 1422 assertions |
|| **Cross-cut** | 5 | **100% (5/5) ✅** | N02 secure parent dir, N05 local trust, N03 key leakage prevention, N04 vendor key derivation. Only N01 token counting remains as known gap |
|| **Build/doc** | 15 | 30% | Cross-compile, Windows, Docker, CI |

### Session 2026-05-22 (This Continuation)

- ✅ **L12: Browse.sh skills hub** — `skill_search_hub()` + `skill_install_from_hub()` with HTTP fetch, JSON parse, query filtering, detail+CDN install, provenance tracking
- ✅ **skill_hub tool registered** — search/install/list actions via skill_hub_handler
- ✅ **skill_search hub:true param** — merges browse.sh results with local, adds `source` field
- ✅ **CLI /skills search-hub <query> / install <slug>** — subcommands in cmd_skills
- ◀ **Upstream drift: 5/12 remain** (L01-L04, L07)
- ◀ **Build/doc: skills hub feature added** — 3 files, +335 lines, 58/58 tests pass
- ◀ Committed: `dbe604c64`
