<p align="center">
  <img src="C/BANNER.md" alt="Slermes C" width="100%">
</p>

# Slermes ☤ — C Translation of Hermes Agent

**Slermes** is a zero-dependency [C translation](C/) of the Python [Hermes Agent](https://github.com/NousResearch/hermes-agent) by Nous Research. One static binary, zero runtime deps beyond libc + libssl.

This repo (`waefrebeorn/slermes`) is a fork of [NousResearch/hermes-agent](https://github.com/NousResearch/hermes-agent) with the C translation living in [`C/`](C/). The Python reference implementation lives at the repo root (tracking upstream).

| Aspect | Python (upstream) | C (Slermes) |
|--------|-------------------|-------------|
| **Location** | Repo root (upstream tracking) | [`C/`](C/) |
| **Binary** | `hermes` (Python + uv) | `slermes` (static ELF, 31M) |
| **Deps** | Python 3.11+, uv, Node.js | libc + libssl only |
| **Suite** | ~17k pytest tests | 282/0/0 C tests |
| **Readme** | [Upstream Hermes Agent docs](https://hermes-agent.nousresearch.com/docs) | [`C/README.md`](C/README.md) |

<p align="center">
  <a href="C/README.md"><img src="https://img.shields.io/badge/Slermes%20C-README-FFD700?style=for-the-badge" alt="Slermes C README"></a>
  <a href="C/.hermes/mind-palace/battleship-v27.md"><img src="https://img.shields.io/badge/Battleship-v27-blue?style=for-the-badge" alt="Battleship v27"></a>
  <a href="C/.hermes/mind-palace/vault/achievements.md"><img src="https://img.shields.io/badge/Vault-Achievements-green?style=for-the-badge" alt="Vault"></a>
  <a href="https://github.com/waefrebeorn/slermes"><img src="https://img.shields.io/badge/Fork-waefrebeorn/slermes-blueviolet?style=for-the-badge" alt="Fork"></a>
</p>

---

## Quick Start (C)

```bash
cd C/
make -j$(nproc)            # Build slermes binary
./slermes --help           # Usage
bash test_runner.sh        # 282/0/0 suite
```

See [C/README.md](C/README.md) for full build docs, architecture, tool list, gateway platforms, and provider support.

## Project Structure

```
waefrebeorn/slermes/
├── C/                          ← Slermes C translation (all source)
│   ├── src/                    ←   174 .c files (agent, tools, CLI, gateway, cron)
│   ├── lib/                    ←   65 library modules (json, http, crypto, mcp, ...)
│   ├── tests/                  ←   239 test files (282/0/0 suite)
│   ├── include/                ←   Master header + subsystem headers
│   ├── .hermes/                ←   Mind-palace docs, battleship, vault
│   │   └── mind-palace/        ←     Walkway files, battleship, achievements
│   └── README.md               ←   Full C build/architecture docs
├── assets/                     ←   Python upstream assets
├── ...python source...         ←   Upstream Python Hermes Agent reference
└── README.md                   ←   This file (Slermes repo entry point)
```

## History

This repo started as a straight fork of `NousResearch/hermes-agent` at commit `2517917de`. All C translation work lives in `C/`. The original 277-commit development history is preserved on the [`c-work`](https://github.com/waefrebeorn/slermes/tree/c-work) branch for reference — squashed into a single commit on `main` for clean upstream tracking.

- **Fork:** [waefrebeorn/slermes](https://github.com/waefrebeorn/slermes)
- **Upstream:** [NousResearch/hermes-agent](https://github.com/NousResearch/hermes-agent)
- **Old dev branch:** [`c-work`](https://github.com/waefrebeorn/slermes/tree/c-work) (277 original C commits)
- **Battleship:** [v27 — 33 parity gaps](C/.hermes/mind-palace/battleship-v27.md)
- **Vault:** [Achievements log](C/.hermes/mind-palace/vault/achievements.md)

## Parity Status

| Metric | Value |
|--------|-------|
| Suite | 282 passed, 0 failed, 0 skipped |
| Tools | 85 registered |
| CLI | 80 commands + 37 config sections |
| Gateway | 19 platforms |
| Providers | 10 (OpenAI, Anthropic, Google, DeepSeek, xAI, Azure, Bedrock, OpenRouter, Custom, Copilot) |
| Libraries | 65 C modules |
| Binary | 31M dynamic ELF |
| Warnings | 0 |
| Stubs | 0 |
| **Real parity gaps** | **33** (5 S0 + 5 S1 + 4 S2 + 6 S3 + 7 S4 drift) |

> **Honest assessment:** "0 gaps" was premature. Real parity gaps exist across form-vs-function, pipeline, cross-comparison, product features, and upstream drift (7583 upstream commits behind). See [battleship v27](C/.hermes/mind-palace/battleship-v27.md).

## Upstream Python

The Python reference implementation is at the repo root (tracking upstream `NousResearch/hermes-agent:main` at `67011cc0d`). For Python usage, install and run the Python Hermes Agent:

```bash
# Python Hermes Agent (upstream)
curl -fsSL https://raw.githubusercontent.com/NousResearch/hermes-agent/main/scripts/install.sh | bash
hermes --help
```

See [upstream docs](https://hermes-agent.nousresearch.com/docs/) for full Python documentation.

---

<p align="center">
  <i>Slermes — a C translation of Hermes Agent by Nous Research.</i><br>
  <a href="C/.hermes/mind-palace/goal-mantra.md">Goal Loops</a> · 
  <a href="C/.hermes/mind-palace/state.md">State</a> · 
  <a href="C/.hermes/mind-palace/plan.md">Plan</a> · 
  <a href="C/.hermes/mind-palace/prestige_prompt.md">Priorities</a>
</p>
