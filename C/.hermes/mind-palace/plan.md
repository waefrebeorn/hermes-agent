# Plan (v146)

155 gaps across 9 sectors (battleship v34). Fork diverged

## Phase 0: Display & Visual (S0, P0) — 2 gaps
Prompt input depth, type-ahead, bounding box re-layout, input scaling/wrapping

## Phase 1: Conversation Loop + Provider Adapters + TUI + Tests (25 gaps)
5 conversation loop partials (L24-L28), provider adapter layer (6), TUI gateway server + core components (14), test coverage campaign

## Phase 2: Test Coverage Campaign (S7, P1) — 20 gap clusters, 1000+ tests
Build out C test suite to match Python coverage (248 files → 1262, 57K LOC → 474K)

## Phase 3: Gateway Helpers + Tool Depth (S3, S6, P2) — ~33 gaps
13 gateway sub-modules, 10 tool depth improvements

## Phase 4: CLI Ecosystem (S5, P2) — ~30 gaps
Setup wizard, doctor, config, models, auth, kanban, skills hub, voice mode, etc.

## Phase 5: Plugin System + Architecture (S9, S10, P2-P3) — ~30 gaps
Plugin architecture + lifecycle, memory/kanban/observability plugins, architecture gaps (Python interop, async, ACP, replay)

## Phase 6: Agent Module Depth (S2, S8, P2) — ~13 gaps
Insights, models_dev, stream_diag, remaining provider adapters
