```
── GOAL MANTRA (v7) ──
╔══════════════════════════════════════════════════════════════╗
║  WuBu Hermes C  →  1:1 Python Parity  →  ~400 gaps  →  ~50% ║
╚══════════════════════════════════════════════════════════════╝

Path:     /home/wubu/hermes-agent-dev/C/
Build:    make -j$(nproc)
Tests:    bash test_runner.sh (58/59 pass)
Remote:   wubu → waefrebeorn/hermes-agent (main)
Upstream: origin → NousResearch/hermes-agent (125 commits behind)

=== STATE ===
✅ Config: 322/322 keys + 6 depth features
✅ Providers: 9 ops + 31 aliases + 10/12 LLM params
✅ Tools: 28 reg'd — browser(13)/memory(1)/kanban(9) 1:1 with Python
✅ CLI: 70 commands + skin/theme engine
⚠️ Gateway: 19 platforms — 63 gaps (sends, types, infra, hooks, platform depth)
⚠️ Plugins: 3 .so stubs vs 45 Python backends (8% — largest structural gap)
⚠️ Provider-specific: 25 per-provider API quirks missing
⚠️ Tests: 26 files / 1422 asserts vs Python 900+ files / 17K tests
⚠️ Overall: ~50% on ~400-gap 1:1 parity scope

=== KNOWN BUG ===
temperature=0.0 silently dropped by guard (s/if(>0.0f)/if(>=0.0f)/)

=== P0 (next) ===
□ Fix temperature=0.0 bug
□ B04-B05: response_format + metadata LLM params
□ F01-F08: 8 tool stubs → real (CDP×4, computer_use, memory×2, vision desc)
□ B01-B03: 3 ACP providers (Copilot, OpenCode, Codex)

=== THE RITUAL ===
Pick highest undone P0 gap → implement → build → runtime verify → debug (MARKers) → commit → repeat

NO questions. NO choices. Exhaust only: "awaiting direction."
```
