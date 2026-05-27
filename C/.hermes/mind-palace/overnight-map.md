# Slermes C — Overnight Map (v34 — ~347 Items)

## Navigation
- **Battleship:** battleship-v16.md — ~347 item-level gaps, 11 layers
- **State:** state.md (v34) — ~43% parity, Phase 1 CLI Args ✅ (117 commands), DA v16 findings documented
- **Phase Order:** Display (14/16) → CLI Args ✅ → Providers → Tools (21) → Missing Tools → Gateways → Agents → Plugins → Libraries → Security → Tests → Config
- **Goal:** goal-mantra.md (v34)

## DA v16 Alert
Phase 2 provider deepen claims are STALE. Verify each against source before implementing.
**New stubs found:** 10 missing Python tools (feishu_drive_comment ×4, video_analyze, yuanbao ×4), 2 agent stubs (llm_background_review unwired, api_server mock fallback).
Recommended: skip Phase 2 deepen entirely, go straight to Phase 3 tool features (21 gaps remaining).

## Phase 0 — Display (16 gaps) — 14/16 done
V01-V06 ✅, V10-V16 ✅. Remaining: V07 TUI, V08 Python TUI, V09 voice.

## Phase 1 — CLI Args (40 gaps) — ✅ ALL DONE
All 117 commands wired with proper argument processing.

## Phase 2 — Provider Parity (~20 real gaps)
All deepen claims stale per DA v16. Real port targets: copilot, copilot-acp, openai-codex, opencode-zen.

## Phase 3 — Tool Features (21 gaps)
Web_get cookies param added (-1). Next: x_search filters (geo/media/user), web_get rate limiting, vision OCR, etc.
