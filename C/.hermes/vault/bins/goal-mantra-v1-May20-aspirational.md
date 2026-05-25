# Goal Mantra — Hermes C Translation (Archived v1, May 20)
**⚠️ OUTDATED — Replaced by goal-mantra.md (May 25 HONEST)**

**Translate Hermes Agent Python → C, zero-dependency single binary.**

Phase 1: Foundation deps (JSON/YAML/HTTP/crypto/DB/display → C wrappers)
Phase 2: Agent core (loop + LLM client + CLI → C)
Phase 3: Tools (file/terminal/web/skills → C)
Phase 4: Gateway (server + platform adapters → C)
Phase 5: Advanced (cron/skills/edge → C)

**Rules:**
- No Python runtime dependency
- Single compiled binary
- Track alongside Python in same repo (C/ dir)
- Every pull: digest → implement → build → commit
- No time goals. Loop until done.