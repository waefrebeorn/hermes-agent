# Overnight (v283)

Phase 216: L26 hermes_message_sanitize() — sanitization pipeline (surrogate fix, think-block strip, secret redaction). Wired into agent_loop.c. 35 tests. 108→107 gaps. S1 L26 PORTED.
Phase 215: L26 estimate_payload_context_tokens() — JSON API payload token estimator. Counts chars in Chat Completions/Responses API shapes, /4 for tokens. 10 tests. 109→108 gaps.
Phase 210: D16 type-ahead — background stdin reader thread, line_edit_set_text() API. 114→113 gaps. S0 all PORTED.
Phase 209: D09 emacs keybindings ported in line_edit.c — Ctrl-A/E/B/F/K/Y/L/T/P/N, Alt-F/B/D. 66-test suite. 115→114 gaps.
Phase 208: tool_coerce_number() + tool_coerce_boolean() ported from Python model_tools. 35 tests. S0 model_tools depth.

Next: S7 test expansion, S1 partials (L24-L28), or B08 send_message remaining depth.
