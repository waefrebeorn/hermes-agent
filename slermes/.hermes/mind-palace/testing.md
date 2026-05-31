# Testing — Slermes C Test Suite (v453)

## Current Status
338/?/13 — Suite stable. Pre-existing TTY skip fluctuation acceptable.

## Test Categories
- Library unit tests (json, protobuf, yaml, crypto, http, ...)
- Provider unit tests (OpenAI, Anthropic, Google, DeepSeek, xAI, Azure, Bedrock, OpenRouter, Custom)
- Tool tests (file, web, terminal, send_message, patch, session_search, ...)
# Testing — Slermes C Test Suite (v455)

## Current Status
338/?/13 — Suite stable.

## Recent Improvements (Phase 399)
- test_provider_openrouter.c: 51→94 assertions (+43)
- 5 new test functions + 1 bug fix (finish_reason in parse_response)

## Test Labels (test_runner.sh)
- provider_openrouter: 94 tests (was 50)
