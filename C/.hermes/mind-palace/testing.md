(v270)

320/0/0, 278 test files. All pass. Gap: 278 C vs ~17k Python.

Phase 203: S7 test expansion — 11 new edge case assertions in test_sanitize.c (nested JSON repair, unicode surrogates, long input, URL token redaction, SSH key path). Assertions 24→35.
Phase 202: S3 G02 + G06 PORTED via function-level reclassification. 115 gaps.
Phase 198: _transform_sudo() wired into terminal_handler() (7 tests). B07 depth.
Coverage: 278/1262 test files (22.0%). 48 new assertions across Phases 196-200.
