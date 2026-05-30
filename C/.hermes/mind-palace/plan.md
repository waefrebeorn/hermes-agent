# Plan — Next Phase (v332)

S0+S1+S3+S6 PORTED. L24+L25+L26+L27+L28 PORTED. F10 PORTED. S8 R01+R10+R04 PARTIAL, R03+R05-R09 WON'T PORT. 95 gaps.

**Next gap targets:**

| Priority | Sector | Gap | Action |
|----------|--------|-----|--------|
| P2 | S0 D09 | Vi mode | . repeat done (Phase 261). % bracket match done (Phase 260). Remaining: / search, visual mode, count prefixes |
| P1 | S7 X01-X09 | Test coverage | Continue expanding test files (292 / 1262 parity). Pending: provider depth, gateway edge cases |
| P1 | S8 R02 | Bedrock depth | stop_reason mapping + 4 utility functions done (Phase 262-263). Remaining: model discovery, credential detection |
| P1 | S8 R04 | Gemini depth | google_is_native_base_url() + google_coerce_content_to_text() done (Phase 264-265). Remaining: translate_tool_call, translate_tool_result, build_gemini_contents, translate_tools, translate_tool_choice, normalize_thinking_config |
| P1 | S4 T01-T18 | TUI backend | JSON-RPC gateway, transport, render, entry |
| P2 | S5 C01-C30 | CLI ecosystem | Setup wizard, doctor, config, profiles |
