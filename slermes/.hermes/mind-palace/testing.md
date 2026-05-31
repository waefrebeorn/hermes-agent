# Testing — Slermes C Test Suite (v453)

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

## Recent Improvements (Phase 397)
- test_provider_azure.c: 54→94 assertions (+40)
- 4 new test functions: URL/header edge, response edge, streaming depth
- Bug fix: finish_reason extraction in azure_parse_response

## Test Labels (test_runner.sh)
- provider_openai: 111 tests
- provider_anthropic: 98 tests
- provider_deepseek: 106 tests
- provider_google: 152 tests
- provider_xai: 133 tests
- provider_azure: 94 tests (was 54)
