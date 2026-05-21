# WuBu Slermes — C Translation Roadmap

**Mission:** 1:1 C port of NousResearch/hermes-agent. Run alongside Python hermes.
**Strategy:** Track upstream daily via `hermes update` + `digest.py`. Never modify Python files in ways that can't be stashed — C/ directory is additive only.
**Binary:** `./C/hermes` (alias `slermes`) — ~809KB, 0 errors, 21/21 tests, makes live DeepSeek calls.

---

## Status Dashboard

| Metric | Value |
|--------|-------|
| **Phase** | Phase 5 (Advanced) |
| **Binary size** | ~809KB ELF 64-bit |
| **Compiler errors** | 0 |
| **Pre-existing warnings** | ~8 (format-truncation, unused params — same as baseline) |
| **Tests** | 21/21 pass (10 lib + 2 plugin + 9 integration) |
| **Tools** | 27 (18 core + 4 browser + 1 security/approval + provider framework) |
| **CLI commands** | 16 (registry-dispatched) |
| **Gateway platforms** | 7 (Telegram, Discord, Slack, Matrix, Mattermost, Webhook, WhatsApp) |
| **Bugs fixed** | 5 (Content-Type header, chunked TE, skin default, .env parser, WEBHOOK_PORT) |
| **Upstream parity** | Python v0.14.0 — C tracks at ~Phase 5 / basic agent parity |
| **Config isolation** | `~/.slermes/` (SLERMES_HOME env) — separate from `~/.hermes/` |
| **Live API** | DeepSeek `deepseek-v4-flash` via `https://api.deepseek.com` — ~1.2s avg |

---

## Architecture

```
hermes-agent-dev/
├── C/                        ← C translation (primary work area)
│   ├── src/
│   │   ├── agent/            ← Agent loop, LLM client, provider interface, context
│   │   ├── cli/              ← CLI orchestrator, config, display, commands (16)
│   │   ├── tools/            ← 27 tool implementations
│   │   ├── gateway/          ← 7-platform gateway
│   │   └── cron/             ← Cron scheduler
│   ├── lib/                  ← 12 standalone libraries
│   ├── include/              ← Headers
│   ├── tests/                ← 21 test sources
│   └── .hermes/mind-palace/  ← Project mind palace (walkway + 5 plan docs)
├── scripts/
│   ├── slermes-sync.sh       ← Pull upstream + digest + report
│   └── slermes-build.sh      ← Clean build + test + parity check
├── ROADMAP.md                ← This file
├── parity_loop.sh            ← C/ ↔ slermes/ 1:1 parity check
├── triple_test.sh            ← hermes + hermes-dev + slermes comparison
└── README.md                 ← Fork overview + C dashboard
```

---

## Phases (170-phase roadmap in mind palace)

| Phase | Area | Status | Detail |
|-------|------|--------|--------|
| 1-10 | Foundation libs | ✅ DONE | json, http, yaml, crypto, dotenv, cron, proc, template, tui, db |
| 11-20 | Agent loop | ✅ DONE | LLM client, streaming, context, agent loop, CLI, config |
| 21-40 | Tools | ✅ DONE | 18 core tools + browser(4) + approval(1) + provider framework |
| 41-50 | Gateway | ✅ DONE | 7 platforms + plugin system + skin engine + cron + TUI |
| 51-60 | CLI parity — commands | ⚡ 16/50+ | Commands registry, /new /undo /history /topic /config /tools /commands |
| 61-70 | CLI parity — config | ⬜ P0 | /personality /verbose /yolo /fast /reasoning |
| 71-80 | CLI entry points | ⬜ P0 | doctor, status, logs, setup, profiles, config, models, backup |
| 81-90 | Browser automation | ⚡ 4/12 | navigate, snapshot, back, forward. Need: click, type, scroll, CDP |
| 91-100 | Security | ⚡ 1/5 | approval tool + agent loop integration. Need: URL safety, path traversal, Tirith |
| 101-110 | Provider interface | ⚡ created | Abstract provider API + OpenAI impl. Need: wire into llm_client, Anthropic, Google |
| 111-120 | Gateway platforms | ⬜ P1 | Signal, Email, SMS, HA, DingTalk, WeCom, Feishu, BlueBubbles |
| 121-130 | Gateway advanced | ⬜ P1 | Multi-platform, session lifecycle, streaming, health, hooks |
| 131-140 | More tools | ⬜ P1 | Kanban(8), Spotify(7), transcription, MCP, checkpoint mgr |
| 141-150 | Advanced features | ⬜ P1-P2 | Skills hub, terminal backends, context compression, checkpoint/resume |
| 151-160 | Testing & CI | ⬜ P2 | 21→100+ tests, API mocks, CI pipeline, fuzz, memcheck |
| 161-170 | Developer experience | ⬜ P2-P3 | Docs site, Nix, Docker, APT packages, telemetry |

Full 170-phase detail: `C/.hermes/mind-palace/plans/roadmap-100-phases.md`

---

## Workflow

### Daily: Sync from upstream + digest for C work

```bash
# One command:
bash scripts/slermes-sync.sh

# Or manually:
git fetch origin main          # Get upstream changes
git merge origin/main          # Merge (may need conflict resolution)
python3 C/digest.py            # Check what C work is needed
bash scripts/slermes-build.sh  # Rebuild + test
```

### C Development Loop

```bash
cd C/
make -j$(nproc)                # Build
bash test_runner.sh            # Test
./hermes "Say hi"              # Smoke test

# Or from root:
bash scripts/slermes-build.sh  # Clean build + test + parity check
```

### Running Side-by-Side

```bash
# Python production
hermes "Hello"

# Python dev
hermes-dev "Hello"

# C binary
slermes "Hello"

# Compare all three
bash triple_test.sh "Your prompt here"
```

---

## Key Design Decisions

1. **Config isolation:** C binary uses `~/.slermes/` (SLERMES_HOME) — never touches `~/.hermes/`. Both can run simultaneously with different API keys, models, and providers.

2. **Additive only:** C/ directory is pure addition to the repo. No Python files are modified for the C translation. `git stash` of C/ changes leaves the Python tree pristine.

3. **Digestion automation:** `python3 C/digest.py` diffs Python source against C stubs and reports gaps. Run after every upstream pull.

4. **Security-first:** Dangerous command approval is enforced at the agent loop level. All terminal/file/web tools check against dangerous patterns before execution. Non-interactive sessions deny dangerous operations by default.

5. **Provider abstraction:** The provider interface (provider.h) allows plugging any LLM API format. Currently supports OpenAI-compatible (covers DeepSeek, OpenRouter, Groq, Together AI). Anthropic and Google providers are designed but not yet implemented.

---

## Related Documents

| Document | Location | Content |
|----------|----------|---------|
| C mind palace | `C/.hermes/mind-palace/` | Full walkway + 5 plan docs |
| Prestige prompt | `C/.hermes/mind-palace/prestige_prompt.md` | Full resume + priority queue |
| Goal mantra | `C/.hermes/mind-palace/goal-mantra.md` | Single pasteable session block |
| 170-phase roadmap | `C/.hermes/mind-palace/plans/roadmap-100-phases.md` | All phases with LOC estimates |
| DA audit v2 | `C/.hermes/mind-palace/plans/devils_advocate_v2.md` | Full 4-phase DA sweep |
| CLI parity spec | `C/.hermes/mind-palace/plans/cli-1to1-parity.md` | 50+ command spec |
| Tool parity spec | `C/.hermes/mind-palace/plans/tool-parity.md` | 40+ tool spec |
| Gateway parity | `C/.hermes/mind-palace/plans/gateway-platform-parity.md` | 20+ platform spec |
| Current state | `C/.hermes/mind-palace/state.md` | Binary truth table |
| Python gap analysis | `C/GAP_ANALYSIS.md` | 165+ feature gaps vs Python |
| Parity loop | `parity_loop.sh` | C/ ↔ slermes/ 1:1 checker |
| Triple test | `triple_test.sh` | hermes + hermes-dev + slermes comparison |
| Dependencies | `C/DEPENDENCIES.md` | Full Python→C dependency map |
| Digestion | `C/digest.py` | Automation for upstream change tracking |
