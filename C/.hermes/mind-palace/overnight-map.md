# Overnight (v281)

Phase 214: L26 tool_call_args_truncate() — JSON arg string truncation preserving validity. Recursive tree walk shrinks long string values, re-serializes. 29 tests. 110→109 gaps.
Phase 213: L25 repair_tool_call() — tool name normalization pipeline. 11 tests. 111→110 gaps.
Phase 210: D16 type-ahead — background stdin reader thread, line_edit_set_text() API. 114→113 gaps. S0 all PORTED.
Phase 209: D09 emacs keybindings ported in line_edit.c — Ctrl-A/E/B/F/K/Y/L/T/P/N, Alt-F/B/D. 66-test suite. 115→114 gaps.
Phase 208: tool_coerce_number() + tool_coerce_boolean() ported from Python model_tools. 35 tests. S0 model_tools depth.

Next: S7 test expansion, S1 partials (L24-L28), or B08 send_message remaining depth.
