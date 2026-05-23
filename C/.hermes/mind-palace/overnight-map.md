# Overnight Map — 2026-05-23 (DA v12)

## Active: computer_use real backend (S01-S02)

**Suite: 154/0/0** (117 tests, ~573 assertions + 10 new computer_use tests)
**Binary: 9.3MB dynamic**
**400 commits, 0 behind upstream**

## What Was Done (May 23)
- **S01-S02**: computer_use backend abstraction implemented — noop backend (10 unit tests), X11 fallback (xdotool/ImageMagick), full 13-action schema, safety checks (blocked keys/types). Replaces 30-line stub with ~900-line implementation.
- **S01-S02 test**: 10 unit tests for noop backend + lifecycle + global backend. Added to test_runner.sh.
- **CDP re-audit**: browser.c is NOT a stub — 1495-line implementation with real CDP WebSocket client, JS execution, screenshot, dialog handling. `stub_cdp_handler` is dead code never registered. DA v11 false ✅ corrected.
- **Header**: `include/hermes_computer_use.h` — new public API with backend vtable, data types, lifecycle helpers.

## P0 Gaps (next session picks first)
1. ~~S01-S02 — computer_use real backend~~ ✅ DONE
2. **U01-U02** — CI gate + Docker build verification (check if CI passes with new files)
3. **S07** — image_gen real backend (Fal AI REST client)

## Verified Numbers (don't re-derive)
- Parity: 33% (163/500), NOT 32%
- Stubs: 2 critical remaining (image_gen, TUI) — CDP was never a stub, computer_use now has real backend
- Tests: 10 new computer_use tests added (11 total with the stubs correction)
- CDP browser: 13 registered tools, all with real handlers. Requires external CDP server (same as Python)

## Fallback
Pick U01 (CI gate) — ensure new files compile in CI, or pick S07 (image_gen) — next highest-impact broken feature.
