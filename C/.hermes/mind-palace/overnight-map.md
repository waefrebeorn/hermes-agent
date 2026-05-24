# Hermes C — Overnight Navigation Map (2026-05-23, Session 53)

## Active: image_routing.py port (391L → C). Parity ~324/500 (~65%).

**Suite: 197/1/0 ✅, Build: 0 errors ✅**
**Commit: `22c4ffb56` — pushed to wubu/main**

## What Was Done (May 23)
- **image_routing (B29):** Port of Python `agent/image_routing.py` (391L → ~350 C + header).
  New module `src/agent/image_routing.c` + `include/image_routing.h`. Functions ported:
  - `decide_image_input_mode()` — config-aware decision (auto|native|text), reads `agent.image_input_mode`
    from config.yaml. In auto mode: checks `auxiliary.vision.provider` override, then
    `model_supports_vision()` from provider_metadata.
  - `build_native_content_parts()` — builds OpenAI-style JSON content array with text parts plus
    `image_url` data URL parts. Includes `[Image attached at: path]` hints in text.
  - MIME sniffing from magic bytes: PNG, JPEG, GIF, WebP, BMP, HEIC/HEIF (all common formats).
  - `file_to_data_url()` — reads file, detects MIME, base64 encodes, builds `data:mime;base64,...` URL.
  - `guess_mime()` — suffix-based fallback with magic byte override.
  - New config field: `agent.image_input_mode` (char[16]) added to `agent_config_t` in hermes.h,
    loaded from YAML `agent.image_input_mode` with default "auto".
  Tests: 34/34 pass. Suite: +0 (no new integration tests added for full binary). Build: 0 errors.

## Next Session Pick
Agent sector is still the biggest gap (45/115, 70 remaining). Top candidates:
1. **background_review.py (587L)** — Background code review for skill pruning via curator fork.
   Medium complexity. Ports: LLM calling pattern (background review loop), string/process management.
2. **skill_manager_tool.py (931L)** — Skill CRUD + curator + hub syncing. Larger but higher impact.
   Ports: tool registration, file operations, curator API calls.

## Key Files
- `C/src/agent/image_routing.c` — implementation
- `C/include/image_routing.h` — public API
- `C/include/hermes.h` — agent_config_t: added image_input_mode field
- `C/src/cli/config.c` — YAML + env loading for image_input_mode
- `C/tests/test_image_routing.c` — 34 tests
- `C/.hermes/mind-palace/goal-mantra.md` — v14 (updated)
- `C/.hermes/mind-palace/prestige_prompt.md` — v14 (updated)
- `C/.hermes/mind-palace/state.md` — updated with Session 53 log
