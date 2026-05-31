# State — Slermes C (v420)
328/0/12. Phase 363: TUI major improvements.
  Thinking indicator: right-aligned on status bar (preserves model info).
  After first token → live token counter (t: 142  tok/s: 34.2).
  Ctrl+C abort during streaming (SIGINT handler + callback check).  
  Non-blocking input polling during entire streaming period.
  Help modal updated with Ctrl+L/Ctrl+C keybind info.
67 gaps. S4 T11 depth improved (thinking indicator + live counter + abort).
