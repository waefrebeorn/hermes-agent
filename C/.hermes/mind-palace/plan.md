# Implementation Plan — Slermes C (v91)

## Phase 0: Form-Not-Function Fixes (P0)
1. **F01** — Intercept unknown flags before LLM dispatch in main.c
2. **F02** — Fix stdin loop in cli.c: split multi-line input, process line by line
3. **F03** — Add missing-arg detection for --session
4. **F04** — Fix tools count display in commands.c

## Phase 1: Missing Tools (P2)
5. **M01-M02** — Discord + Discord Admin tools (HTTP API wrappers)
6. **M03-M07** — Yuanbao tools (SDK-dependent)

## Phase 2: Tool Depth (P2)
7. **D01-D13** — Feature additions per function-level audit

## Phase 3: Library + Gateway (P2)
20. **L02** — MCP SSE persistent streaming
21. **G02** — send_reaction vtable wiring

## Phase 4: Infra + Tests (P3)
22. I01 — Dockerfile
23. I02 — CI workflows
24. X01-X05 — Test expansions
