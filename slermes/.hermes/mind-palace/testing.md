# Testing — Slermes C Test Suite (v448)

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

## Recent Improvements (Phase 392)
- test_provider_openai.c: 37→111 assertions (+74)
- New coverage: multi-tool calls, empty/null content, streaming edge cases,
  build_request_body (basic + tools), URL edge cases, finish_reason parsing
- Bug fixes: finish_reason extraction, reasoning_content streaming

## Test Labels (test_runner.sh)
- provider_openai: 111 tests (was 54)
