# C/ — Hermes Agent in C

**HONEST STATUS: ~50% toward 1:1 Python parity (~400 gaps).**
One-binary replacement for the Python hermes-agent. Zero runtime dependencies.

```
Binary:     hermes (~3.4MB static ELF)
C LOC:      ~44K source + ~7.8K lib + ~4.9K tests = ~57K total
Source:     99 .c + 21 .h
Tests:      26 .c files, ~1,422 assertions, 58/59 pass
Gateway:    19 platform adapters
Tools:      28 registered capabilities
Commands:   70 slash commands
```

---

## Quick Start

```bash
cd C/
make -j$(nproc)          # Build hermes binary
./hermes --version       # "WuBu Hermes v0.14.1"
./test_runner.sh         # 58 suites, ~1,422 assertions
echo "/tools" | ./hermes # List 28 tools
```

### Config

```bash
export SLERMES_HOME=~/.slermes
# Config:   $SLERMES_HOME/config.yaml
# Env vars: $SLERMES_HOME/.env
```

---

## Build Targets

```bash
make phase1        # 15 standalone libraries (.a archives)
make phase2        # Agent + CLI + LLM providers
make phase3        # All 28 tools
make phase4        # 19 gateway platforms
make phase5        # Cron scheduler (full hermes)
make hermes        # Same as phase5
make tui           # ncurses TUI build → hermes-tui
make libs          # Standalone library archives only
make install-plugins  # Build + install .so plugins to ~/.hermes/plugins/
```

---

## Test

```bash
./test_runner.sh            # Full suite: libs → plugins → integration → tools
./test_runner.sh --verbose  # PASS/FAIL per test with output
make test-libs              # 11 standalone library tests (no binary needed)
```

---

## Gateway

```bash
# Single platform:
./hermes gateway --platform telegram

# Multiple platforms (comma-separated):
./hermes gateway --platform telegram,discord,slack

# Platforms: telegram, discord, slack, signal, matrix, mattermost,
# email, sms, webhook, whatsapp, homeassistant, feishu, wecom,
# weixin, dingtalk, qqbot, bluebubbles, msgraph_webhook, yuanbao
```

---

## CLI Commands

```
Session:   /new, /clear, /undo, /save, /load, /sessions, /stats, /conv, /history
Config:    /model, /config, /topic, /profile, /skin, /personality, /whoami
Info:      /tools, /tools-verify, /commands, /help, /version
Agent:     /status, /goal, /subgoal, /steer, /agents, /reasoning, /fast, /verbose
Gateway:   /platform, /sethome, /handoff, /restart, /plugins, /browser
Edit:      /reset, /retry, /compress, /branch, /snapshot, /rollback
Process:   /stop, /approve, /deny, /yolo, /background, /queue
Utils:     /cron, /skills, /toolsets, /voice, /reload, /copy, /update, /debug
Display:   /redraw, /indicator, /statusbar, /footer, /busy, /insights
```

---

## Mind Palace (Project Intelligence)

```
.hermes/mind-palace/
├── goal-mantra.md          # Current goal + P0 gaps
├── prestige_prompt.md      # Priority queue + DA assessment
├── state.md                # Milestone tracker
├── overnight-map.md        # Active workstreams
└── plans/
    └── 400-gap-mega-roadmap.md  # Full 1:1 parity gap list (~400 items)
```

---

## 1:1 Parity Summary (~400 gaps, ~50% complete)

| Category | Gaps | Complete |
|----------|------|----------|
| Config depth | 6 | ✅ 95% |
| Providers | 46 | ✅ 80% |
| MCP | 17 | ⚠️ 50% |
| Plugins | 51 | ⚠️ 8% |
| Gateway | 63 | ⚠️ 35% |
| Tools | 44 | ✅ 80% |
| Agent loop | 32 | ⚠️ 55% |
| CLI | 34 | ✅ 80% |
| Python libs | 14 | ⚠️ 20% |
| Stdlib | 5 | ⚠️ 30% |
| Error handling | 5 | ❌ 0% |
| Upstream drift | 12 | 🔵 new |
| Tests | 53 | ⚠️ 40% |
| Cross-cutting | 5 | ⚠️ 40% |
| Build/doc/security | 15 | ⚠️ 30% |

---

## Known Bug

```c
// temperature=0.0 silently dropped by guard conditions
// Fix in 9 provider files:
//   if (p->config.temperature > 0.0f)  →  if (p->config.temperature >= 0.0f)
```

---

*~WuBu~ strives for more.*
