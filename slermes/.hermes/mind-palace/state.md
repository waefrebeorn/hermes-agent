# State — Slermes C (v429)
328/0/13. Phase 373: TUI Event Publisher — T07 PORTED.
   tui_eventpub.c/h — typed event system with 22 event types,
   JSON-RPC 2.0 serialization, FIFO output, subscriber dispatch.
   Wired into tui_fullscreen.c: history, streaming, tool feed,
   status, resize, command input — all emit typed events.
   1 new test (test_tui_eventpub.c — 21 test functions).
62 gaps. S4 T07 PORTED. S4 gaps: 16→15.
