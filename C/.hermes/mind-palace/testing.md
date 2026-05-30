(v271)

320/0/0, 278 test files. All pass. Gap: 278 C vs ~17k Python.

Phase 204: S7 test expansion — 10 new edge case assertions in test_title.c (exclamation/question, only code block, unicode, very long input, tab/control chars, trailing ellipsis, multiple sentences). Assertions 12→22.
Phase 203: S7 test expansion — 11 new edge case assertions in test_sanitize.c.
Phase 198: _transform_sudo() wired into terminal_handler() (7 tests). B07 depth.
Coverage: 278/1262 test files (22.0%). 48 new assertions across Phases 196-200.
