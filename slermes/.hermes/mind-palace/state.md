# State — Slermes C (v436)
335/0/13. Phase 380: TUI WebSocket Support — T04 PORTED.
   tui_websocket.c/h — WebSocket server wrapper for TUI: server
   lifecycle (start/stop/port), client acceptance (accept/send/recv/
   close/is_connected), wraps libwebsocket server API.
   libwebsocket extended: ws_server_listen/accept/close/port added,
   ws:// plain connections supported (was wss:// only), ws_send/recv/
   close now use write_raw/read_raw helpers for SSL/plain auto-dispatch.
   4-test suite (test_tui_websocket.c: start_stop, accept_nonblock,
   port_zero, restart).
55 gaps. S4 T04 PORTED. S4 gaps: 9→8.
   All S4 P1 gaps resolved. S4 remaining: 8 P2-P3 gaps (T19-T28).
