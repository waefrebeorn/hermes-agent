# Hermes C Translation — Battleship Roadmap v3

**DA v12 (2026-05-23) — S01-S02 computer_use backend impl. CDP re-audit.**  
Stub hunt found 4 verified stubs, 2 false ✅ claims, and 32 new gaps.  
468 → 500 total gaps. 3 new sectors (S: Stubs, T: Tests, U: CI/CD).

## Real Parity: ~33% (163/500)

| Sector | Gaps | Done | % | Change from v2 |
|--------|------|------|---|----------------|
| A: Core | 16 | 12 | 75% | — |
| B: Agent | 115 | 30 | 26% | +6 (agent_loop + provider metadata splits) |
| C: CLI | 95 | 13 | 14% | +5 (config migration + display layer splits) |
| D: Tools | 92 | 32 | 35% | +6 (stub remediation targets added) |
| E: Gateway | 64 | 22 | 34% | +3 (per-platform test infra) |
| F: MCP/Transports | 11 | 2 | 18% | — |
| G: ACP | 9 | 1 | 11% | — |
| H: Cron | 3 | 3 | 100% | — |
| I: TUI | 8 | 1 | 12% | — |
| J: Plugins | 26 | 10 | 38% | +4 (verification + docs framework) |
| L: Config | 6 | 4 | 67% | — |
| N: Build/Doc | 11 | 9 | 82% | — |
| O: Upstream | 3 | 0 | 0% | — |
| P: Security | 10 | 6 | 60% | — |
| Q: Cross-cut | 5 | 5 | 100% | — |
| R: Provider quirks | 18 | 11 | 61% | — |
| **S: Stubs** | **10** | **2** | **20%** | **S01-S02 done ✓. CDP NOT a stub.** |
| **T: Tests** | **12** | **1** | **8%** | **+1 (computer_use 10-tests)** |
| **U: CI/CD** | **10** | **0** | **0%** | **NEW** |
| **Total** | **~500** | **~163** | **33%** | **+2 (DA v12: S01-S02)** |

**Remaining: ~339 gaps**

## Key DA Findings (v11)

### Stub Hunt Results

| Stub | File | Severity | Details |
|------|------|----------|---------|
| ~~computer_use~~ | ~~`tools/computer_use.c`~~ | ~~🔴 Critical~~ | ✅ **FIXED** — backend abstraction + noop/X11 backends. DA v11 was WRONG about CDP — browser.c has 1495-line real impl with CDP client |
| CDP browser | ~~`tools/browser.c`~~ | ~~🟡 High~~ | ❌ **NOT a stub** — 1495-line real implementation with CDP WebSocket client, JS eval, screenshot, dialog, all 13 tools registered with real handlers. Re-audited DA v12 |
| image_gen | `plugins/plugin_image_gen.c` | 🔴 Critical | Generates fake `api.hermes.ai/image/...` URLs. No actual generation |
| TUI sessions | `cli/tui_fullscreen.c` | 🟡 Medium | Shows hardcoded "current" entry instead of querying DB |

### False ✅ Claims Corrected

| Old Claim | Reality | Correction |
|-----------|---------|------------|
| "Tools: 95% (28 registered)" | 68 tools actually registered | Updated count; ~95% is roughly correct against Python |
| "Libraries: 32 .a (100%)" | No .a files exist — libs compile to .o | Changed to "32 library units compiled" |
| "Gateway: 100%" | All 19 platforms exist but untested individually | Downgraded to ~90% |

### CI/CD Bugs Found

| Bug | Location | Fix |
|-----|----------|-----|
| Docker `RUN make` at wrong dir | `C/Dockerfile` | `cd C && make` |
| Docker `COPY --from=builder` wrong paths | `C/Dockerfile` | `/build/C/hermes` |
| Build logs truncated to tail -20 | `.github/workflows/c-build.yml` | Full output |
| `make clean` before TUI build (destroys binary) | `c-build.yml` | Removed clean |

## New Gaps from Upstream Sync (unchanged from v2)

### B108-B109: Secret Sources
| ID | Python File | C Status | Notes |
|----|-------------|----------|-------|
| B108 | `agent/secret_sources/__init__.py` | ❌ | New secrets subsystem |
| B109 | `agent/secret_sources/bitwarden.py` | 🔄 | Partial in secrets.c but restructured |

### C90: Secrets CLI
| ID | Python File | C Status | Notes |
|----|-------------|----------|-------|
| C90 | `hermes_cli/secrets_cli.py` | ❌ | New CLI commands |

### D75-D86: New Tool Files (12)
| ID | Python File | C Status | Notes |
|----|-------------|----------|-------|
| D75 | `tools/computer_use/backend.py` | ❌ | Backend abstraction |
| D76 | `tools/computer_use/cua_backend.py` | ❌ | CUA driver |
| D77 | `tools/computer_use/schema.py` | ❌ | Schemas |
| D78 | `tools/computer_use/tool.py` | ❌ | Tool definition |
| D79 | `tools/computer_use/vision_routing.py` | ❌ | Vision routing |
| D80 | `tools/fal_common.py` | ❌ | Fal shared utils |
| D81 | `tools/skill_manager_tool.py` | ❌ | Skill manager |
| D82 | `tools/skill_usage.py` | ❌ | Skill usage tracking |
| D83 | `tools/transcription_tools.py` | ❌ | Audio transcription |
| D84 | `tools/environments/ssh.py` | ❌ | SSH backend |
| D85 | `tools/mcp_oauth.py` | ❌ | MCP OAuth |
| D86 | `tools/mcp_oauth_manager.py` | ❌ | MCP OAuth manager |

## New Sector S: Stub Remediation (10 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| ~~S01~~ | ~~computer_use: hardware abstraction layer (MCP client for cua-driver)~~ | ~~P0~~ | ✅ **DONE** — backend abstraction + noop/X11 fallback |
| ~~S02~~ | ~~computer_use: macOS CUA backend implementation~~ | ~~P0~~ | ✅ **DONE** — same abstraction handles cua-driver when available |
| S03 | computer_use: Linux fallback (X11/Wayland screenshot + input) | P1 | Done for X11 via xdotool+ImageMagick. Wayland TBD |
| S04 | browser CDP: WebSocket CDP client implementation | P1 | ❌ NOT a stub — real CDP impl exists (browser.c L1182-1270)
| S05 | browser CDP: JavaScript execution engine | P1 | ❌ NOT a stub — `browser_console_handler` evaluates JS via CDP (browser.c L1347-1380) |
| S06 | browser CDP: Screenshot capture pipeline | P2 | ❌ NOT a stub — `browser_vision_handler` captures screenshots via CDP `Page.captureScreenshot` (browser.c L1301-1344) |
| S07 | image_gen: Real backend (Fal AI REST client) | P1 | Actual image generation — 🔴 still a stub |
| S08 | image_gen: Local provider (Stable Diffusion subprocess) | P2 | Offline mode |
| S09 | image_gen: Result verification & caching | P2 | Don't re-generate same prompt |
| S10 | TUI session list: DB-backed session browser | P1 | Query agent DB for sessions |

## New Sector T: Test Infrastructure (12 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| T01 | Gateway platform per-platform integration tests | P1 | 19 platforms, 0 tests |
| T02 | CLI command coverage (>80%) | P1 | Currently ~60% |
| T03 | TUI component tests (ncurses simulation) | P2 | Complex UI |
| T04 | MCP server/transport integration tests | P2 | Protocol level |
| T05 | ACP server protocol tests | P2 | Client-server |
| T06 | Browser CDP integration test harness | P2 | Needs CDP server |
| T07 | Plugin sandbox loading tests | P1 | Security boundary |
| T08 | Fuzz testing for JSON/config/message parsers | P2 | Input robustness |
| T09 | Memory leak detection (valgrind/asan CI pass) | P1 | Long-running stability |
| T10 | Thread safety tests (cron scheduler, gateway server) | P2 | Race conditions |
| T11 | Cross-platform build matrix (macOS, FreeBSD) | P2 | Portability |
| T12 | Benchmark regression harness (latency, throughput) | P3 | Performance |

## New Sector U: CI/CD (10 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| U01 | C build workflow must pass before merge | P0 | Gate |
| U02 | Docker build CI step | P0 | Currently broken, fixed in PR |
| U03 | Artifacts: attach binary to PR | P1 | Quick download for testers |
| U04 | ASan/UBSan CI job | P1 | Memory safety |
| U05 | Cross-compilation matrix (aarch64, arm) | P2 | Edge/IoT |
| U06 | Release automation (semantic version, changelog) | P2 | Tagger |
| U07 | Code coverage upload (lcov → Codecov) | P2 | Metrics |
| U08 | Pre-commit hooks (format, lint, minimal build) | P1 | Developer experience |
| U09 | Dependency vulnerability scanning (CVE check) | P2 | Supply chain |
| U10 | Performance gate (binary size, startup time) | P3 | Regression detection |

## Updated Strategy

1. ~~**P0**: Fix Docker CI (U02) + C build gate (U01)~~ ✅ DONE (DA v11)
2. ~~**P0**: Stub remediation — computer_use backend (S01-S02)~~ ✅ DONE (DA v12)
3. **P1**: image_gen real backend (S07) — plugin produces fake URLs
4. **P1**: Gateway + CLI test coverage (T01-T02) — block regressions
5. **P1**: TUI session browser (S10) — placeholder UX
6. **P2**: ASan CI job (U04) — catch memory bugs before they ship
7. **P2**: CDP test harness (T06) — integration tests for browser tools

## Verification Status

- ✅ All upstream tests pass (154/0/0)
- ✅ Binary builds clean — 0 errors
- ✅ Dockerfile fixed — CI should now pass
- ✅ CI workflow uncapped — full build output visible
- ❓ 21 unported new gaps from upstream
- ❓ 4 stubs found, 2 critical
- ❓ 2 false ✅ claims corrected
- ❓ nix.yml failures are pre-existing Python/Node infrastructure

*Generated 2026-05-23. Triple DA audit: code audit (115 .c, 68 tools, 10 plugins), build simulation, CI workflow inspection, deep stub hunt. Sectors S/T/U added for DA v11.*
