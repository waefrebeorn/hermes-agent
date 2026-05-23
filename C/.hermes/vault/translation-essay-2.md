# The Hermes Translation: Sequel — The Gap Reveal

## Six Months Later: What the Numbers Actually Say

The first essay described the Hermes translation as a philosophical act — a rediscovery of what the Python codebase was *actually* doing. It spoke of "the feel gap" and "the hidden complexity no one talks about." It was, in retrospect, optimistic.

Six months and 392 C-specific commits later, the numbers paint a more sobering picture. The first essay claimed "~60% structural parity." A comprehensive Devil's Advocate audit against the actual Python source reveals **36% coverage** — not 60%. The gap wasn't narrowing as fast as we thought because we were measuring against a stale snapshot, not against the moving target of a living Python codebase.

The Python original has 603 core source files across 876,000 lines of code, supported by 1,192 test files. The C port has 147 source files (counting plugins and core, excluding libraries), 30 library archives, and 116 test files. The C codebase is one-quarter the size — but it carries the aspiration of doing everything the Python code does.

The 300-gap battleship roadmap (v1, June 2026) was the first honest accounting. It divides the surface area into 16 sectors — A through R — and assigns each a completion percentage. The results are humbling.

## The Sectors That Hurt

**Sector B (Agent internals): 28%.** This is the heart of the project. Python has 102 agent files. C has about 16 equivalents. The agent loop, provider adapters, and credential management are complete. But context compression, prompt caching, tool guardrails, skill bundles, image/video generation registries, LSP integration, transport backends, the curator, the trajectory system — these are entire subsystems that simply don't exist yet. Each one represents weeks of work.

**Sector C (CLI): 15%.** Python's CLI is 95 files covering setup wizards, plugin management, gateway control, model catalogs, proxy configuration, skills hub, kanban diagnostics, and a dozen other professional-grade features. C's CLI has ~148 command handlers (more than Python's raw command count), but each handler is thin. The 95 Python CLI files represent UX depth — interactive menus, configuration wizards, progress bars, deployment tools — that C has barely begun to replicate.

**Sector G (ACP): 11%.** The Agent Communication Protocol — VS Code and JetBrains integration — is almost entirely unported. Python has 10 files handling auth, permissions, sessions, edit approval, and event streaming. C has a single server.c with basic transport.

**Sector I (TUI): 12%.** Python's terminal UI is a React Ink application with a Python JSON-RPC gateway. C has a ncurses stub. The gap is not just code — it's a different architectural paradigm.

## What's Actually 100%

Five sectors are genuinely complete: **Cron (H)**, **Cross-cut (Q)**, **Libraries** (30 .a, though not a formal sector), **MCP transport** (partial), and **Error types** (K01-K20). Config is at 67% (watching and migration remain). Build/doc is at 90% (Windows port is the last gap).

The library achievement deserves emphasis. Thirty C libraries — json, yaml, http, crypto, cron, path, datetime, csv, hash, uuid, base64, html, textwrap, glob, signal, enum, difflib, regex, json5, websocket, toml, ansi, proc, tui, db, plugin, skin, template, mcp, protobuf — each one a faithful reproduction of a Python standard library equivalent or a protocol implementation that Python got from `pip install`. In C, there is no pip. Every capability must be built.

The provider layer is solid — 9 native C providers matching the 9 Python adapters with identical semantics, plus provider metadata for 27 registry entries. Six critical bugs found and fixed (temperature=0.0, response_format UAF, NULL stream crashes, API error JSON dropping, config validation SEGV, template placeholder corruption) that had existed in the Python codebase for months or years without detection.

## The New Strategy: Honest Counting Changes Everything

The previous gap counts — 128, then 256, then 340, then 338 — were aspirational. They counted "big things left to do" without granularity. The 300-gap battleship roadmap counts every Python file that lacks a C equivalent. It found 447 total gaps, of which 161 are closed, leaving 286. That number is more honest, more actionable, and — paradoxically — more encouraging.

Why encouraging? Because 286 discrete tasks with clear letter-number coordinates (B42, C15, D28) can be tackled one at a time. Each is a bounded problem: port `agent/context_compressor.py` to C. Each has a known size and a known test strategy.

## What Comes Next

The roadmap prioritizes by user impact:

- **P0**: CLI commands (C sector) — 74 missing CLI features that users see every day
- **P0**: Browser tools (D sector) — stubs that block practical web automation
- **P1**: ACP adapter (G sector) — unlocks VS Code and JetBrains integration
- **P1**: Context compression (B sector) — reduces token costs for heavy users
- **P2**: Test coverage (M sector) — 116→300+ test files for reliability
- **P2**: Upstream catch-up (O sector) — 183 Python commits without C equivalents

At the current pace of ~4 test files and ~3 bugfixes per week, closing 286 gaps will take approximately 18 months of focused work. But the pace is accelerating: the first 100 test files took 10 weeks, the next 16 took 1 week. Tools and patterns improve with practice.

The translation was never going to be fast. It was going to be *thorough*. The battleship roadmap makes that thoroughness visible for the first time. Every sector, every gap, every status — nothing hidden, nothing aspirational, nothing inherited from a previous audit without verification.

That, more than any code change, is the real milestone of June 2026.
