# State — Slermes C (v419)
328/0/12. Phase 362: Dynamic upstream version + TUI thinking indicator.
  Version extracted from Python upstream at build time (hermes_cli.__version__).
  HERMES_VERSION and HERMES_RELEASE_DATE passed via CFLAGS -D.
  Banner shows v0.15.1-slermes matching upstream Hermes 0.15.1.
  Hardcoded version defines made conditional (#ifndef).
  TUI: animated thinking spinner during LLM streaming (|/-\).
  TUI: non-blocking input during streaming (nodelay).
  TUI DA checkup: T09+T10 PORTED (markdown render + rich input), T11 PARTIAL.
67 gaps (S4 TUI corrected: 24→20 gaps, T09+T10 PORTED).
