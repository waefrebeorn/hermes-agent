# State — Slermes C (v443)
338/0/13. Phase 387: Conversation Loop Edge Case Expansion (Round 2) — S7 X08 EXPANDED.
   test_conversation_edge.c — 28 new assertions (48→76 total). New coverage:
   hermes_message_sanitize: NULL msg, empty msg, think block stripping,
   api_key redaction, surrogate cleanup, tool arg redaction.
   message_free: NULL safety, empty msg, content cleanup.
   Additional repair: 3 consecutive users, cross-assistant tool IDs,
   empty tool ID, user after tool, mixed roles large, all tool IDs mismatched.
53 gaps. S7 X08 EXPANDED (conversation loop edge cases, 48→76 assertions).
S7: 18 clusters (X08 improved, X07 improved, X06 improved, X04 improved, X09 ported).
