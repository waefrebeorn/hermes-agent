# Overnight Map — Recent Phases (v310)

| Phase | Change | Before | After |
|-------|--------|--------|-------|
|| 219+ | L28 PORTED + tool_delay. S7 depth: test_cli_paths 21 tests, test_session_id 19 tests, test_acp_events 76 tests, test_hermes_signal 8 tests. F10 PORTED. Suite 325/0/0, 280 test files. | 105 gaps | 103 gaps |
|| 239 | S8 R01 adaptive thinking + model-aware max_tokens + beta headers + sampling param forbiddance for Anthropic provider (provider_anthropic.c 731→1085 LOC). 28-provider test suite. Suite 335/0/0, test files 289. | 103 gaps | 102 gaps |
|| 240 | S7 X09 Bedrock provider depth expansion — test_bedrock_depth.c 14→45 tests. build_url, inferenceConfig, system, stop seqs, parse_response (text/tool_use/error/multi-block/null). Suite 335/0/0. | 102 gaps | 102 gaps |
|| 241 | S7 X09 Google provider depth expansion — test_google_depth.c 8→45 tests. build_url, headers, genConfig, multi-msg, parse_response (text/fc/error/blocked). Suite 335/0/0. | 102 gaps | 102 gaps |
|| 242 | S7 X09 Azure provider depth expansion — test_azure_depth.c 10→55 tests. build_url edges (NULL/trailing slash), headers, genConfig (15 params), tools, tool_calls, parse_response/stream all ops. Suite 335/0/0. | 102 gaps | 102 gaps |
|| 243 | S7 X09 OpenRouter provider depth expansion — test_openrouter_depth.c 12→60 tests. build_url, headers (Bearer/Referer/Title), genConfig (14 params), tools, parse_response (text/tc/error/reasoning), stream. Suite 335/0/0. | 102 gaps | 102 gaps |
| 218 | L24 checkpoint/snapshot reclassified PORTED | PARTIAL (106 gaps) | PORTED (105 gaps) |
| 217 | L27 prompt builder PORTED + S7 test expansion (22 assertions) | 107 gaps | 106 gaps |
| 216 | L26 hermes_message_sanitize() — sanitization pipeline | 108 gaps | 107 gaps |
| 215 | L26 estimate_payload_context_tokens() ported | 109 gaps | 108 gaps |
| 214 | L26 tool_call_args_truncate() ported | 110 gaps | 109 gaps |
| 213 | L25 repair_tool_call() ported | 111 gaps | 110 gaps |
| 212 | L25 sanitize_tool_call_arguments() ported | 112 gaps | 111 gaps |
| 211 | L25 message sequence repair ported | 113 gaps | 112 gaps |
| 210 | D16 type-ahead reader — background stdin capture | 114 gaps | 113 gaps |
| 209 | D09 emacs keybindings — 66 tests | 115 gaps | 114 gaps |
