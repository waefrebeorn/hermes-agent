# State — Slermes C (v442)
338/0/13. Phase 386: Conversation Loop Edge Case Expansion — S7 X08 EXPANDED.
   test_conversation_edge.c — 19 new assertions (29→48 total). New coverage:
   consecutive assistants (post-merge count check), assistant before system,
   duplicate tool call IDs, system-only/assistant-only/tool-only message lists,
   null tool name in assistant, long (60-char) tool call IDs, NULL safety
   (repair/sanitize with NULL msgs/count), negative count, zero count.
53 gaps. S7 X08 EXPANDED (conversation loop edge cases, 29→48 assertions).
S7: 18 clusters (X08 improved, X07 improved, X06 improved, X04 improved, X09 ported).
