# Overnight Map — Recent Phases (v284)

| Phase | Change | Before | After |
|-------|--------|--------|-------|
| 218 | L24 checkpoint/snapshot reclassified PORTED | PARTIAL (106 gaps) | PORTED (105 gaps) |
| 217 | L27 prompt builder PORTED + S7 test expansion (22 assertions) | 107 gaps | 106 gaps |
| 216 | L26 hermes_message_sanitize() — sanitization pipeline (surrogate fix, think-block strip, secret redaction). Wired into agent_loop.c. 35 tests. | 108 gaps | 107 gaps |
| 215 | L26 estimate_payload_context_tokens() ported — JSON token estimator | 109 gaps | 108 gaps |
| 214 | L26 tool_call_args_truncate() ported — truncate long tool args | 110 gaps | 109 gaps |
| 213 | L25 repair_tool_call() ported — tool name normalization + Levenshtein fuzzy match | 111 gaps | 110 gaps |
| 212 | L25 sanitize_tool_call_arguments() — corrupt arg repair + marker insertion | 112 gaps | 111 gaps |
| 211 | L25 message sequence repair — drop stray tools, merge consecutive users | 113 gaps | 112 gaps |
| 210 | D16 type-ahead reader — background stdin capture | 114 gaps | 113 gaps |
| 209 | D09 emacs keybindings — Ctrl-A/E/B/F/K/Y/L/T/P/N, Alt-F/B/D. 66 tests | 115 gaps | 114 gaps |
