# Hermes C — Overnight Navigation Map (2026-05-27)

## Session 60 complete — 1 gap closed (B15).

## Session 60 Completed
- **B15: context_engine.py (211L)** — Pluggable context engine interface (vtable). `context_engine_t` struct with function pointers. Default implementations for all optional methods. 11 unit tests. Suite: 214/0/0.
- **B50: web_search_registry.py (262L)** — Thread-safe provider registry for web search/extract/crawl backends. Capability-based resolution, config overrides, legacy preference walk. 10 unit tests.
- **B27: image_gen_registry.py (145L)** — Thread-safe provider registry with register/list/get/get_active/reset. FAL provider registered as default. 6 unit tests. Suite: 212/0/0.
- **B48: video_gen_registry.py (117L)** — Thread-safe provider registry mirroring Python's dict-based map with register/list/get/get_active/reset. FAL provider registered as default. 6 unit tests. Suite: 211/0/0.
- **K12: nous portal tags + reasoning defaults** — portal_tags.c dead code wired into provider_openai.c. Auto-injects `{"tags": [...], "reasoning": {"enabled": true, "effort": "medium"}}`.
- **K12 infra** — portal_tags.c added to all 14 provider depth test compilation blocks in test_runner.sh
- **Q04: ASCII table formatting** — `display_table()` renders bordered, column-aligned tables with Unicode box-drawing. Auto-width, terminal-clamped, colored. Wired into `/commands`.
- **C12: /logs --follow/-f** — tail -f mode with log rotation detection. Polls every 1s.
- **N03: README metrics update** — Parity ~63%, gaps ~176, CLI 79 cmds, suite 210/0/0 in <60s.
- **B37: HTTP proxy env auto-detection** — http_new_with_retry() reads HTTPS_PROXY > HTTP_PROXY > ALL_PROXY on construction.

## Current Reality
- Suite: **210/0/0 in <60s** (T01 done)
- CLI: **79 real cmds** (Q02-Q04 done), tab completion, line editing, table output
- Stubs: **0** (S01/CDP fixed)
- K12 (nous provider): done
- Proxy auto-detect: done
- **~175 gaps remaining** (down from ~270)

## Next Workstreams (pick one)
**A — Agent modules (~43 gaps, P0):** Smallest: stream_diag.py (280L), context_references.py (518L), image_gen_registry.py (145L)
**B — Provider plugins (~17 gaps, P1):** Add config defaults + metadata for remaining providers
**C — Gateway (~12 gaps, P1):** Per-platform integration tests

## Data Not to Re-Derive
- CLI commands.c: 79 real cmds, 0 stubs. Tab completion + history + table output work.
- Battleship-v4.md is canonical reference — not the stale goal banner.
- Test suite: all 14 provider depth tests pass (portal_tags.c wired in).
- git push: main up to date.
