# Testing — Slermes C Test Suite (v450)

## Current Status
338/?/13 — Suite stable. Pre-existing TTY skip fluctuation acceptable.

## Test Categories
- Library unit tests (json, protobuf, yaml, crypto, http, ...)
- Provider unit tests (OpenAI, Anthropic, Google, DeepSeek, xAI, Azure, Bedrock, OpenRouter, Custom)
- Tool tests (file, web, terminal, send_message, patch, session_search, ...)
- TUI tests (json_rpc, transport, render, layout, entry, slash_worker, eventpub, websocket, edge)
- Agent loop tests (agent_loop_core, conversation_edge, fuzz)
- Gateway platform tests (gateway_platforms)
- Process, tool coercion, delegate edge case tests

## Recent Improvements (Phase 394)
- test_provider_deepseek.c: 71→106 assertions (+35)
- New coverage: URL edge cases, header edge cases (neg cache TTL), tool calls,
  reasoning+tool combined, streaming edge cases, FIM response/body edge cases,
  empty/no/null choices

## Test Labels (test_runner.sh)
- provider_openai: 111 tests (was 54)
- provider_anthropic: 98 tests (was 74)
- provider_deepseek: 106 tests (was 60)
