# Plan — Next Phase (v455)

S0-S3+S6+S8 all PORTED. S4: all P1 gaps resolved.
53 gaps. Suite 338/?/13.

**Latest:** Phase 399 — OpenRouter Provider Test Expansion (S7 X03).
test_provider_openrouter.c: 43 new assertions (51→94 total). URL edge
(double-slash fix, proxy, empty base), header edge (empty/NULL/long key),
response edge (empty/no choices, null content, length finish, no usage),
streaming reasoning (reasoning_content in delta), streaming edge depth
(empty delta/chunk, length finish, whitespace).
Bug fix: openrouter_parse_response — finish_reason extraction.

**Next gap target:**
- S7 X03 — Custom provider (last of 10)
- S4 T19-T28, S5 CLI, S9 plugins
