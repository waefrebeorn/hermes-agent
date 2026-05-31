# Testing — Slermes C Test Suite (v453)

## Current Status
338/?/13 — Suite stable. Pre-existing TTY skip fluctuation acceptable.

## Test Categories
- Library unit tests (json, protobuf, yaml, crypto, http, ...)
- Provider unit tests (OpenAI, Anthropic, Google, DeepSeek, xAI, Azure, Bedrock, OpenRouter, Custom)
- Tool tests (file, web, terminal, send_message, patch, session_search, ...)
# Testing — Slermes C Test Suite (v454)

## Current Status
338/?/13 — Suite stable. Pre-existing TTY skip fluctuation acceptable.

## Recent Improvements (Phase 398)
- test_provider_bedrock.c: 41→102 assertions (+61)
- 6 new test functions: stop reason mapping, error classification,
  context overflow, response edge cases, URL edge, streaming depth

## Test Labels (test_runner.sh)
- provider_openai: 111 tests
- provider_anthropic: 98 tests
- provider_deepseek: 106 tests
- provider_google: 152 tests
- provider_xai: 133 tests
- provider_azure: 94 tests
- provider_bedrock: 102 tests (was 40)
