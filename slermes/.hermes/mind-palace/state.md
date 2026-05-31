# State — Slermes C (v431)
330/0/13. Phase 375: TUI Entry/Startup Module — T05 PORTED.
   tui_entry.c/h — wraps TUI lifecycle: pre-flight checks (TERM,
   isatty, color support), SIGTERM handler, startup result codes,
   exit reason tracking. Graceful degradation paths with clear
   error messages for terminal too small, no color, no TERM.
   1 new test (test_tui_entry.c — 10 test functions).
60 gaps. S4 T05 PORTED. S4 gaps: 14→13.
