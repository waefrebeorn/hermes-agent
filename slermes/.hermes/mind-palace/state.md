# State — Slermes C (v433)
332/0/13. Phase 377: TUI Transport Layer — T02 PORTED.
   tui_transport.c/h — abstracted FIFO-based transport with connection
   state machine (IDLE/CONNECTING/CONNECTED/DISCONNECTED/ERROR),
   configurable reconnection, state change callbacks, message framing
   (newline-delimited), poll-based I/O, send/recv API, send_rpc
   convenience, sendf formatted output, shutdown cleanup.
   13-test suite (test_tui_transport.c).
58 gaps. S4 T02 PORTED. S4 gaps: 12→11.
