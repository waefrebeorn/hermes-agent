<p align="center">
  <a href="slermes/README.md">
    <img src="slermes/assets/slermes-logo-clean.svg" alt="Slermes" width="400">
  </a>
</p>

# Slermes ☤

<p align="center">
  <a href="slermes/README.md"><img src="https://img.shields.io/badge/Slermes-README-FFD700?style=for-the-badge" alt="README"></a>
  <a href="slermes/.hermes/mind-palace/battleship-v34.md"><img src="https://img.shields.io/badge/Battleship-v34-0066FF?style=for-the-badge" alt="Battleship v34"></a>
  <a href="slermes/.hermes/mind-palace/vault/achievements.md"><img src="https://img.shields.io/badge/Vault-achievements-00AA55?style=for-the-badge" alt="Vault"></a>
  <a href="https://github.com/waefrebeorn/slermes"><img src="https://img.shields.io/badge/Fork-waefrebeorn/slermes-8B5CF6?style=for-the-badge" alt="Fork"></a>
  <a href="https://github.com/NousResearch/hermes-agent"><img src="https://img.shields.io/badge/Upstream-NousResearch-FF6B35?style=for-the-badge" alt="Upstream"></a>
  <img src="https://img.shields.io/badge/Tests-325%2F0%2F14-00AA55?style=for-the-badge" alt="Tests">
  <img src="https://img.shields.io/badge/Binary-31MB%20ELF-FFD700?style=for-the-badge" alt="31MB">
</p>

**A zero-dependency C translation of [Hermes Agent](https://github.com/NousResearch/hermes-agent) by Nous Research.** One static binary, zero runtime deps beyond libc + libssl. 98 CLI commands, 85 tools, 19 gateway platforms, 10 LLM providers — all in 31MB of pure C.

<table>
<tr><td><b>⚡ Pure C, no runtime</b></td><td>One static ELF binary. No Python, no Node.js, no uv, no venv. `./slermes` and it runs. libc + libssl is all you need.</td></tr>
<tr><td><b>🔧 85 native tools</b></td><td>File ops, terminal, web search, browser automation, MCP, cron, kanban, image generation, TTS, voice, Home Assistant, Discord — all compiled in, no plugin hell.</td></tr>
<tr><td><b>🌐 19 gateway platforms</b></td><td>Telegram, Discord, Slack, WhatsApp, Signal, Matrix, Email, SMS, Feishu, WeCom, DingTalk, QQ Bot, BlueBubbles, YuanBao, Webhook, Home Assistant — one daemon, zero interpreter overhead.</td></tr>
<tr><td><b>🧠 10 LLM providers</b></td><td>OpenAI, Anthropic, Google, DeepSeek, xAI, Azure, Bedrock, OpenRouter, Custom, Copilot — native adapters, OpenAI-compat fallback. Switch with `/model`.</td></tr>
<tr><td><b>📦 65 zero-dep libraries</b></td><td>JSON, YAML, TOML, HTTP/HTTPS, MCP, cron, regex, protobuf, WebSocket, Base64, UUID, CSV, HTML, crypto (AES/SHA/HMAC), line editing, ANSI, templating — all hand-written C, no external deps.</td></tr>
<tr><td><b>🧪 325-test suite</b></td><td>289 test files, completes in &lt;60s. 0 warnings. 0 stubs.</td></tr>
</table>

---

## Quick Start

```bash
git clone https://github.com/waefrebeorn/slermes.git
cd slermes/slermes/
make -j$(nproc)            # Build in ~30s
./slermes --help           # See what it can do
bash test_runner.sh        # 325/0/14 suite
```

**Docker** (no build tools needed):
```bash
docker build -t slermes -f slermes/Dockerfile .
docker run --rm slermes --help
```

---

## Why C?

| | Python (upstream) | C (Slermes) |
|---|---|---|
| **Runtime** | Python 3.11+, uv, Node.js | **libc + libssl** |
| **Startup** | ~500ms (interpreter + uv) | **~5ms** |
| **Binary size** | 150MB+ (venv + deps) | **31MB static ELF** |
| **Suite speed** | ~45 min (17k pytest) | **&lt;60s** (325 tests) |
| **Memory idle** | ~180MB | **~4MB** |
| **Deploy** | uv sync, venv, Python | **`scp slermes`** |

---

## Project Map

```
waefrebeorn/slermes/
├── slermes/                    ← C translation (all source)
│   ├── src/                    ← 174 .c files (agent, tools, CLI, gateway)
│   ├── lib/                    ← 65 library modules (self-contained C)
│   ├── tests/                  ← 239 test files (325/0/14)
│   ├── include/                ← Master header + subsystem APIs
│   ├── assets/                 ← Mascot, logos, banners
│   ├── extras/                 ← C toolchest: img2svg, palette, asciimg, morph, imggrid
│   ├── .hermes/                ← Mind-palace docs, battleship, vault
│   └── README.md               ← Full C build/architecture docs
├── ...python source...         ← Upstream Python Hermes Agent (tracking)
└── README.md                   ← This file
```

---

## Parity Status

325 tests pass | 68 gaps remain | 8 active sectors

The [battleship](./slermes/.hermes/mind-palace/battleship-v34.md) tracks every feature gap between C and Python — function-level API comparisons, not file-name guessing. The [vault](./slermes/.hermes/mind-palace/vault/achievements.md) logs every resolved gap and retired stale claim.

**PORTED sectors:** S0 (display/input), S1 (agent core), S2 (CLI/config), S3 (gateway), S6 (tools), S8 (providers), S10 (infrastructure)
**ACTIVE sectors:** S4 (TUI — 2 gaps), S5 (agent modules — 15 gaps), S7 (test coverage — 20 gaps), S9 (ecosystem — 20 gaps)

---

## Upstream

This is a fork of [NousResearch/hermes-agent](https://github.com/NousResearch/hermes-agent). The Python reference lives at the repo root. C translation tracks upstream's architecture while being entirely independent code.

```bash
# Upstream Python (if you want the original)
curl -fsSL https://raw.githubusercontent.com/NousResearch/hermes-agent/main/scripts/install.sh | bash
```

---

<p align="center">
  <a href="slermes/.hermes/mind-palace/goal-mantra.md">Goal Loops</a> ·
  <a href="slermes/.hermes/mind-palace/state.md">State</a> ·
  <a href="slermes/.hermes/mind-palace/plan.md">Plan</a> ·
  <a href="slermes/.hermes/mind-palace/prestige_prompt.md">Priorities</a>
</p>
