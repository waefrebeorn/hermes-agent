(v278)

323/0/0, 279 test files. All pass. Gap: 279 C vs ~1262 Python.

Phase 208: tool_coerce_number/boolean ported from Python model_tools (35 tests). S0 depth.
Phase 207: tool_error_sanitize() ported from Python model_tools. Strips XML/CDATA/code fences. 26 tests.
Phase 206: agent_get_continuation_prompt() ported from Python conversation_loop. 24 tests.
Phase 205: strip_yaml_frontmatter() ported from Python agent/prompt_builder.py. Added to lib/libhtml (7 new assertions in test_html.c). S1 L27 prompt builder depth — new standalone utility.
Phase 204: S7 test expansion — 10 new edge case assertions in test_title.c.
Phase 198: _transform_sudo() wired into terminal_handler() (7 tests). B07 depth.
Coverage: 278/1262 test files (22.0%). 121 new assertions across Phases 196-208.
