# Testing — Slermes C Test Suite (v451)

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

## Recent Improvements (Phase 395)
- test_provider_google.c: 65→152 assertions (+87)
- 8 new test functions: finish reason mapping (9 Google reasons),
  content blocked, is_native_base_url, coerce_content_to_text,
  URL/header edge cases, streaming finish reason depth, empty candidates

## Test Labels (test_runner.sh)
- provider_openai: 111 tests
- provider_anthropic: 98 tests
- provider_deepseek: 106 tests
- provider_google: 152 tests (was 64)
