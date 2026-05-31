# State — Slermes C (v422)
328/0/12. Phase 366: TUI type-ahead buffer (T18) — keystrokes captured during streaming, injected after stream end.
   Previously just beeped and discarded. Now buffers up to 1024 chars in stream_state_t.type_ahead_buf
   and replays into input buffer when streaming finishes.
66 gaps. S4 T18 IMPROVED from PARTIAL to PORTED (type-ahead buffer + replay).
