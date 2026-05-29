# Plan (v145)

205+ gaps across 9 sectors (battleship v34). Fork diverged — C/ lives only on fork; upstream removed C/.

## Phase 0: Display & Visual (S0, P0) — 12 gaps
Skin engine, spinner, banner, status bar, tool feed, response box, help tables, 24-bit color, prompt input, markdown render, faces, emoji

## Phase 1: Agent + Adapters + TUI Core (S1, S3, S7, P1) — ~38 gaps
Missing agent modules, provider adapter layer, TUI gateway server + core components

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
