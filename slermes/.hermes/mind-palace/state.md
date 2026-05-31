# State — Slermes C (v440)
338/0/13. Phase 384: Agent Loop Core Function Tests — S7 X06 EXPANDED.
   test_agent_loop.c — 90 new assertions covering: session_id format,
   agent_free (zeroed/messages/snapshot), agent_configure_from_config
   (full mapping/partial/NULL safety), agent_inject_history
   (valid/NULL/empty/invalid JSON/tool role/append), agent_snapshot
   (empty/with messages/double take/restore).
53 gaps. S7 X06 EXPANDED (agent loop core tests, 90 assertions).
S7: 18 clusters (X06 improved, X04 improved, X09 ported).
