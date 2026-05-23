# Hermes C — Overnight Navigation Map (2026-06-07, Session 52)

## Active: error_classifier.py port (1134L → C). Parity ~323/500 (~65%).

**Suite: 197/1/0 ✅, Build: 0 errors ✅**
**Commit: `6aa136580` — pushed to wubu/main**

## What Was Done (Jun 7)
- **error_classifier (B28):** Port of Python `agent/error_classifier.py` (1134L → 811 C + 115 header).
  Full 8-step classification pipeline: provider-specific (Anthropic thinking, llama.cpp grammar, xAI Grok),
  HTTP status code (401/402/403/404/413/429/500/503/529), error code matching, message-only pattern
  matching (billing/rate_limit/context/auth/timeout/SSL/disconnect), server-disconnect + large-session
  context overflow heuristic, JSON body extraction (nested error.message, param, code).
  Tests: 25/25 pass. Suite: +1 (197/1/0).
- **DA audit:** All 14 pattern arrays verified exact match vs Python. 1 known deviation documented:
  no metadata.raw JSON unwrapping for OpenRouter-wrapped upstream errors (medium impact).

## Next Session Pick
Agent sector is the biggest gap (44/115, 71 remaining). Top candidates:
1. **image_routing.py (391L)** — Image routing for native vision support. Config + capability-based routing.
   Medium complexity, mostly logic (no complex deps).
2. **background_review.py (587L)** — Background code review for skill pruning via curator fork.
   Medium complexity, string/process management.

## Key Files
- `C/lib/liberrorclassifier/error_classifier.h` — public API
- `C/lib/liberrorclassifier/error_classifier.c` — implementation
- `C/tests/test_error_classifier.c` — 25 tests
- `C/.hermes/mind-palace/state.md` — updated with Session 52 log
- `C/.hermes/mind-palace/prestige_prompt.md` — v13 (updated)
- `C/.hermes/mind-palace/goal-mantra.md` — v13 (updated)
