(v280)

**Slermes C** — Full C translation of Hermes Agent by Nous Research.

Suite: 323/0/0 | 85 tools | 98 CLI | 37 config sections | 19 GW | 10 prov | 65 libs | 280 test files

Phase 213: L25 repair_tool_call() ported — tool name normalization pipeline (lowercase, hyphens→underscores, CamelCase→snake_case, _tool suffix, Levenshtein fuzzy match). 11 tests. 110 gaps remain.
Phase 212: L25 sanitize_tool_call_arguments() ported (22 tests).
Phase 211: L25 hermes_repair_message_sequence() ported (17 tests).
