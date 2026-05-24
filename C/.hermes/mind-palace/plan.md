# Implementation Plan — Hermes C Translation (2026-05-24 v15)

**Phase: Battleship reset, fresh gap count. Next: tackle biggest gaps.**

## Phase 1: CLI Depth Attack (P0)
| Step | What | Target |
|------|------|--------|
| 1.1 | Fix 197 stub commands in commands.c | Replace printf(...) with real implementations |
| 1.2 | Add readline support (history, C-r search) | Replace fgets() with libedit/readline |
| 1.3 | Tab autocomplete for commands | CLI completion |
| 1.4 | Rich output formatting (panels, tables) | ANSI/colour formatting |

## Phase 2: Agent Module Ports (P0)
| Step | What | Lines | Priority |
|------|------|-------|----------|
| 2.1 | agent_init.py → C | 1638L | P0 |
| 2.2 | agent_runtime_helpers.py → C | 2189L | P0 |
| 2.3 | prompt_builder.py → C | ~500L | P0 |
| 2.4 | chat_completion_helpers.py → C | ~400L | P1 |
| 2.5 | tool_executor.py → C | ~300L | P1 |
| 2.6 | auxiliary_client.py → C | 5289L | P1 |
| 2.7 | account_usage.py → C | 326L | P1 |
| 2.8 | context_compressor.py → C | ~400L | P1 |
| 2.9 | context_engine.py → C | ~500L | P1 |
| 2.10 | shell_hooks.py → C | 847L | P1 |

## Phase 3: Provider Plugin Expansion (P1)
| Step | What | Priority |
|------|------|----------|
| 3.1 | OpenRouter client tool port | P1 |
| 3.2 | Azure Foundry provider | P1 |
| 3.3 | Copilot/Copilot-ACP provider | P2 |
| 3.4 | Remaining 16 provider plugins | P2 |

## Phase 4: Gateway Coverage (P1)
| Step | What | Priority |
|------|------|----------|
| 4.1 | api_server gateway (REST API) | P1 |
| 4.2 | WeCom callback + crypto helpers | P2 |
| 4.3 | Feishu comment support | P2 |
| 4.4 | Yuanbao media/protobuf/sticker | P2 |

## Phase 5: Test Infrastructure (P1)
| Step | What | Priority |
|------|------|----------|
| 5.1 | Fix suite timeout — parallelize or optimize | P1 |
| 5.2 | Gateway per-platform integration tests | P1 |
| 5.3 | Provider API mock tests | P1 |
| 5.4 | CLI dispatch tests | P1 |
| 5.5 | Memory leak CI (valgrind gate) | P1 |
| 5.6 | E2E inference test | P1 |

## Phase 6: TUI & Polish (P2)
| Step | What |
|------|------|
| 6.1 | Response box wrapping |
| 6.2 | Config editor (TUI) |
| 6.3 | Theme engine depth |
| 6.4 | Image viewer |

## Phase 7: CI/CD & Build (P2)
| Step | What |
|------|------|
| 7.1 | Cross-compile matrix in CI |
| 7.2 | Release automation |
| 7.3 | Code coverage upload |
| 7.4 | CVE scanning |

## Dependency Graph

```
Phase 1 (CLI) ← no blockers
Phase 2 (Agent) ← depends on: agent_init.py understanding
Phase 3 (Providers) ← depends on: provider.c extension
Phase 4 (Gateway) ← no blockers
Phase 5 (Tests) ← depends on: Phase 1-4 implementations (for integration)
Phase 6 (TUI) ← no blockers
Phase 7 (CI/CD) ← no blockers
```

Phase 1 and 2 can run in parallel. Phase 3 needs provider infrastructure in place.
