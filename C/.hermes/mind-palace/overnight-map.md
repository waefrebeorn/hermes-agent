# Overnight (v150)

154 gaps (battleship v34 across 9 sectors). Fork diverged — C/ lives only on fork; upstream removed C/.
Suite 294/0/0 (248 test files). Phase 66: stream drop diagnostics. Phase 65: upstream header capture (cf-ray, x-openrouter-*, x-request-id) wired into non-streaming LLM path. Structured stream error logging. Phase 64: error_classify() wired. S2 stale sweep: A15 retired.

## Current Phase: Phase 6 (S2 Agent Module Depth)
2 real implementable S2 gaps: A18 (models_dev — model management CLI), A22 (stream_diag — stream diagnostics, partial). A15 retired as stale (already ported: usage_pricing.c + cmd_insights). error_classify() wired into non-streaming LLM path. All S0 remaining, S1 partials stable.

## Priority stacking
- Upstream feature-gap methodology applied to all future claims
- S2 depth (insights, models_dev) → S6 tool depth (vision, web, terminal depth) → S3 gateway helpers
