# Overnight Map — 2026-05-23 (DA v13)

## Active: T01+T02 done (172 new assertions). 168/500 parity.

**Suite: 154/0/0** (117 tests, ~573 assertions)
**Binary: 9.3MB dynamic**
**401 commits, 0 behind upstream**

## What Was Done (May 23)
- **T01**: Gateway per-platform tests — Telegram JSON parsing (is_mentioned, is_group, get_chat_id, get_text, get_update_type), Discord setters, Webhook HMAC + subscription CRUD. 64 assertions.
- **T02**: CLI dispatch/handler tests — commands_get_all, commands_dispatch, /exit handler, /help output, count invariants, list_json validation. 108 assertions.
- **Battleship cleanup**: D75-D79, U01, U02, U04 marked DONE (already implemented from prior sessions)

## P0 Gaps (next session picks first)
1. ✅ T01-T02: Gateway + CLI test coverage
2. ✅ ASan CI job (U04)
3. **T07**: Plugin sandbox loading tests (P1 security)
4. **T09**: Memory leak detection (valgrind/asan CI pass)
5. **U03**: Attach binary to PR artifacts
6. **U08**: Pre-commit hooks
7. **S03**: computer_use Wayland fallback

## Verified Numbers (don't re-derive)
- Parity: 34% (168/500)
- All 4 DA v11 stubs resolved
- U01-U02, U04 CI gates already implemented
- D75-D79 computer_use backports already done via S01-S02

## Fallback
Pick T07 (Plugin sandbox loading tests) — next highest P1 security gap.
