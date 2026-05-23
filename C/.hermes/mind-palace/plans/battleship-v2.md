# Hermes C Translation — Battleship Roadmap v2

**DA v10 (2026-05-23) — Triple source verified against upstream sync.**  
21 new gaps from 183 upstream commits merged. 447 → 468 total gaps.

## Real Parity: ~34% (161/468)

| Sector | Gaps | Done | % | Change from v1 |
|--------|------|------|---|----------------|
| A: Core | 16 | 12 | 75% | — |
| B: Agent | 109 | 30 | 28% | +2 (secret_sources) |
| C: CLI | 90 | 13 | 14% | +1 (secrets_cli) |
| D: Tools | 86 | 32 | 37% | +12 (computer_use, fal, ssh, mcp_oauth, transcription) |
| E: Gateway | 61 | 22 | 36% | — |
| F: MCP/Transports | 11 | 2 | 18% | — |
| G: ACP | 9 | 1 | 11% | — |
| H: Cron | 3 | 3 | 100% | — |
| I: TUI | 8 | 1 | 12% | — |
| J: Plugins | 22 | 10 | 45% | +5 (fal image/video, discord plugin, openviking) |
| L: Config | 6 | 4 | 67% | — |
| N: Build/Doc | 11 | 9 | 82% | +1 (parallel test runner) |
| O: Upstream | 3 | 0 | 0% | — |
| P: Security | 10 | 6 | 60% | — |
| Q: Cross-cut | 5 | 5 | 100% | — |
| R: Provider quirks | 18 | 11 | 61% | — |
| **Total** | **~468** | **~161** | **34%** | **+21 gaps from upstream** |

**Remaining: ~307 gaps** (286 in v1 + 21 new from upstream sync)

## Key DA Findings (v10)

**Source 1 — Code audit (115 .c files, 117 tests, 32 libs, 10 plugins):**
- 154/0/0 suite, 573 assertions
- Source LOC: 75,563 (non-lib)
- Test-to-source ratio: 117 tests / 115 source = 1.02:1
- No regressions from upstream merge

**Source 2 — Upstream sync delta (183 commits merged):**
- 22 new Python files, 164 modified
- 74 non-test Python files need C attention
- Biggest-impact new gaps: computer_use expansion (5 files), Fal AI (2 files), Discord plugin platform (2 files), secrets subsystem (2 files)
- 55 modified files need C equivalent review — some may need no changes if the Python changes were minor

**Source 3 — Test coverage verification:**
- Libraries fully tested (32/32 .a)
- Plugins all tested (10/10)
- Gateway platforms: no per-platform tests (stub test only)
- CLI: thin coverage (commands, paths, display)
- Cron: 4 test files covering lib, tool, sqlite store, extras (retry/chain/template)
- Agent core: partial — loop, context, budget, checkpoint, title, redact, sanitize, vault all tested
- Provider errors: 225 assertions across 9 providers
- Tools: 28 tools registered, most have tests

## New Gaps from Upstream Sync

### B108-B109: Secret Sources (new subsystem)
| ID | Python File | C Status | Notes |
|----|-------------|----------|-------|
| B108 | `agent/secret_sources/__init__.py` | ❌ | New secrets subsystem — abstracts credential sourcing |
| B109 | `agent/secret_sources/bitwarden.py` | 🔄 | Bitwarden provider — partially in C's secrets.c but restructured |

### C90: Secrets CLI
| ID | Python File | C Status | Notes |
|----|-------------|----------|-------|
| C90 | `hermes_cli/secrets_cli.py` | ❌ | New — secrets management CLI commands |

### D75-D86: New Tool Files (12)
| ID | Python File | C Status | Notes |
|----|-------------|----------|-------|
| D75 | `tools/computer_use/backend.py` | ❌ | New — computer use backend abstraction |
| D76 | `tools/computer_use/cua_backend.py` | ❌ | New — CUA backend driver |
| D77 | `tools/computer_use/schema.py` | ❌ | New — computer use schemas |
| D78 | `tools/computer_use/tool.py` | ❌ | New — computer use tool definition |
| D79 | `tools/computer_use/vision_routing.py` | ❌ | New — vision routing for computer use |
| D80 | `tools/fal_common.py` | ❌ | New — Fal AI shared utilities |
| D81 | `tools/skill_manager_tool.py` | ❌ | New — skill manager tool |
| D82 | `tools/skill_usage.py` | ❌ | New — skill usage tracking |
| D83 | `tools/transcription_tools.py` | ❌ | New — audio transcription |
| D84 | `tools/environments/ssh.py` | ❌ | New — SSH terminal backend |
| D85 | `tools/mcp_oauth.py` | ❌ | New — MCP OAuth |
| D86 | `tools/mcp_oauth_manager.py` | ❌ | New — MCP OAuth manager |

### J18-J22: New Plugin Backends (5)
| ID | Python Plugin | C Status | Notes |
|----|--------------|----------|-------|
| J18 | `plugins/image_gen/fal/` | ❌ | New — Fal AI image generation plugin |
| J19 | `plugins/video_gen/fal/` | ❌ | New — Fal AI video generation plugin |
| J20 | `plugins/platforms/discord/` | ❌ | New — Discord gateway plugin platform |
| J21 | `plugins/memory/openviking/` | ❌ | New — OpenViking memory provider |
| J22 | (transcription tools) | ❌ | New — transcription-related plugin work |

### N11: Parallel test runner
| ID | File | C Status | Notes |
|----|------|----------|-------|
| N11 | `scripts/run_tests_parallel.py` | ❌ | New — C test runner has no parallel equivalent |

## 55 Modified Files Needing Review

Key files where C equivalents may need updates:
- **Agent core**: `anthropic_adapter`, `conversation_loop`, `chat_completion_helpers`, `codex_responses_adapter`, `error_classifier`, `file_safety`, `model_metadata` — C equivalents exist but may need sync
- **CLI**: `commands.py`, `config.py`, `main.py`, `setup.py`, `env_loader.py` — C has ~148 handlers but Python's CLI structure changed
- **Gateway**: `run.py`, `telegram.py`, `webhook.py` — C gateways need review
- **Tools**: `file_tools.py`, `image_generation_tool.py`, `skills_guard.py`, `skills_hub.py`, `voice_mode.py`, `x_search_tool.py`
- **Plugin**: `hermes_cli/plugins_cmd.py` — C plugin system needs sync

## Updated Strategy

1. **P0**: Close new upstream gaps in sector D (computer_use expansion — 5 files) — these are heavily used features
2. **P1**: Port upstream secrets subsystem (B108-B109) — affects credential management
3. **P1**: Discord platform plugin (J20) — enables plugin-based gateway adapters
4. **P2**: Review 55 modified files for C equivalent drift
5. **P2**: Parallel test runner (N11) — suite takes 2+ minutes

## Verification Status

- ✅ All upstream tests pass (154/0/0)
- ✅ Binary builds clean
- ✅ 0 merge conflicts — clean rebase
- ❓ 21 new gaps unported
- ❓ 55 modified files need review for C equivalent drift

*Generated 2026-05-23. Triple source: code audit, upstream diff, test suite.*
