# Plan (v145)

214+ gaps across 10 sectors (battleship v34). Fork diverged

## Phase 0: Display & Visual (S0, P0) — 2 gaps
Prompt input depth, type-ahead, bounding box re-layout, input scaling/wrapping, type-ahead

## Phase 1: Conversation Loop Plumbing + Agent Modules + Adapters + TUI Core (S1, S2, S8, S4, P1) — ~76 gaps
17 conversation loop plumbing gaps (5 done), agent modules, provider adapter layer, TUI gateway server + core components

## Phase 2: Test Coverage Campaign (S6, P1) — 20 gap clusters, 1000+ tests
Build out C test suite to match Python coverage (248 files → 1262, 57K LOC → 474K)

## Phase 3: Gateway Helpers + Tool Depth (S2, S5, P2) — ~33 gaps
13 gateway sub-modules, 10 tool depth improvements

## Phase 4: CLI Ecosystem (S4, P2) — ~30 gaps
Setup wizard, doctor, config, models, auth, kanban, skills hub, voice mode, etc.

## Phase 5: Plugin System + Remaining Agent Modules (S8, S1 P2-P3) — ~46 gaps
Plugin architecture, memory/model/kanban plugins, small agent modules

## Phase 6: Architecture + Platform Gaps (S9) — ~7 gaps
Python interop, async, ACP protocol, credential automation
