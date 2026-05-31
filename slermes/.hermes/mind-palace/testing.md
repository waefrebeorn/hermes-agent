# Testing — Slermes C Test Suite (v449)

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

## Recent Improvements (Phase 393)
- test_provider_anthropic.c: 28→98 assertions (+70)
- New coverage: response parsing (text, tool_use, multi-tool, empty, null, error, malformed)
- Streaming: text_delta, content_block_start/stop, message_delta, message_start, ping, raw JSON
- URL edge cases: trailing slash, custom proxy, already has /messages
- Header edge cases: NULL key, OAuth token, cached

## Test Labels (test_runner.sh)
- provider_openai: 111 tests (was 54)
- provider_anthropic: 98 tests (was 74)
