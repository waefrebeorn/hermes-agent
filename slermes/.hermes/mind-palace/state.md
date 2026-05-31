# State — Slermes C (v435)
334/0/13. Phase 379: TUI Render Engine — T03 PORTED.
   tui_render.c/h — virtual screen with dirty-rect tracking, double
   buffering, text wrapping/scrolling, color/attribute stacks,
   markdown render pipeline (**bold**, `code`, # headers, indented
   code blocks, role-based coloring), flush to ncurses with
   incremental dirty-region update.
   16-test suite (test_tui_render.c).
56 gaps. S4 T03 PORTED. S4 gaps: 10→9.
