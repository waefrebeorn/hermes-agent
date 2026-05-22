# Slermes C — Overnight Map (May 21 session)

## What Changed This Session
- **P87: Tool call parallelism** — pthread-based parallel tool dispatch (non-Windows), sequential fallback
- **P89: Interrupt handling** — SIGINT handler, clean shutdown, agent_free unregisters
- **P90: Smart context eviction** — 3 strategies: tool-first, user-pairs, keep-N. Token estimation API
- Commits: c013c43 (P87), f2e3de2 (P89), 11679e3 (P90)
- All pushed to wubu=waefrebeorn/hermes-agent main

## What's Next (P91-P100: Agent loop depth)
- **P91: System prompt caching** — deduplicate system prompt across turns
- **P92: Prefill messages** — Anthropic-style prefill support
- **P93: Tool result classification** — error/fatal/success marking
- **P94: Reasoning content** — depth (already stored in message_t)
- **P95-P100:** Streaming diagnostics, compression feedback, checkpoints, init, background review
