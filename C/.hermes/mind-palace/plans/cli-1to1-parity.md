# Hermes C — CLI 1:1 Parity Spec (May 21 AM)

## Current: 16 commands, 4 entry points
## Target: 50+ commands, 194 entry points (matching Python)

## 1. Slash Commands (Interactive Mode)

### Session Lifecycle — C has 7/9
| Command | Python | C | Priority | 
|---------|--------|---|----------|
| /new | ✅ | ✅ | done |
| /reset | ✅ | ✅ P1 | P0 |
| /clear | ✅ | ✅ | done |
| /undo | ✅ | ✅ | done |
| /retry | ✅ | ❌ | P0 |
| /save | ✅ | ✅ | done |
| /load | ✅ | ✅ | done |
| /sessions | ✅ | ✅ | done |
| /stats | ✅ | ✅ | done |
| /conv | ✅ | ✅ | done |
| /history | ✅ | ✅ | done |
| /topic | ✅ | ✅ | done |
| /branch | ✅ | ❌ | P1 |
| /snapshot | ✅ | ❌ | P1 |
| /handoff | ✅ | ❌ | P1 |
| /compress | ✅ | ❌ | P1 |

### Configuration — C has 1/12
| /model | ✅ | ✅ | done |
| /config | ✅ | ❌ | P0 |
| /personality | ✅ | ❌ | P1 |
| /verbose | ✅ | ❌ | P1 |
| /yolo | ✅ | ❌ | P1 |
| /fast | ✅ | ❌ | P1 |
| /reasoning | ✅ | ❌ | P1 |
| /skin | ✅ | ❌ | P1 |
| /statusbar | ✅ | ❌ | P2 |
| /footer | ✅ | ❌ | P2 |
| /indicator | ✅ | ❌ | P2 |
| /voice | ✅ | ❌ | P2 |

### Tools & Skills — C has 0/10
| /tools | ✅ | ❌ | P0 |
| /toolsets | ✅ | ❌ | P1 |
| /skills | ✅ | ❌ | P1 |
| /plugins | ✅ | ❌ | P1 |
| /cron | ✅ | ❌ | P1 |
| /bundles | ✅ | ❌ | P2 |
| /reload | ✅ | ❌ | P1 |
| /reload-mcp | ✅ | ❌ | P1 |
| /reload-skills | ✅ | ❌ | P1 |
| /curator | ✅ | ❌ | P1 |
| /kanban | ✅ | ❌ | P1 |

### Info — C has 0/10
| /help | ✅ | ✅ | done |
| /commands | ✅ | ❌ | P0 |
| /usage | ✅ | ❌ | P1 |
| /insights | ✅ | ❌ | P2 |
| /platforms | ✅ | ❌ | P1 |
| /platform | ✅ | ❌ | P1 |
| /update | ✅ | ❌ | P2 |
| /debug | ✅ | ❌ | P1 |
| /copy | ✅ | ❌ | P2 |
| /paste | ✅ | ❌ | P2 |
| /image | ✅ | ❌ | P2 |

## 2. CLI Entry Points (Non-Interactive)
C has: `hermes`, `hermes --version`, `hermes gateway`, `hermes cron`, `hermes --tui`
Python: 194 CLI entry points via click/argparse

### Must-Have P0 (Phase 71-80)
| Entry | Description | Est LOC |
|-------|-------------|---------|
| hermes doctor | System health check | 300 |
| hermes status | Runtime status dashboard | 200 |
| hermes logs | Log viewer with filters | 300 |
| hermes setup | Interactive setup wizard | 500 |
| hermes profiles | Profile management | 300 |
| hermes config | Config editor CLI | 400 |
| hermes models | Model/provider management | 400 |
| hermes tools | Tool listing | 200 |
| hermes backup | Backup/restore | 300 |
| hermes update | Self-update | 200 |

### Nice-to-Have P1-P2
| Entry | Description |
|-------|-------------|
| hermes chat | Explicit chat mode |
| hermes gateway --platform X | Multi-platform gateway |
| hermes gateway --all | All platforms simultaneously |
| hermes cron list/add/remove | Cron management |
| hermes checkpoints | Checkpoint management |
| hermes mcp | MCP server commands |
| hermes plugins list/install | Plugin management |
| hermes skills hub/sync | Skill management |
| hermes kanban | Kanban board CLI |
| hermes uninstall | Self-uninstall |

## 3. Implementation Plan

### Phase 1 (CLI infrastructure)
- Add command registry with descriptions, aliases, help text
- Implement /help to list all commands with pagination
- Add argument parsing for subcommands

### Phase 2 (Session commands: /new /reset /undo /retry)
- Extend session DB to support undo/retry
- Add context branching for /branch
- Add checkpoint/snapshot for /snapshot

### Phase 3 (Config commands: /config /personality /verbose /yolo /fast)
- Extend config system for runtime changes
- Add personality/system prompt management
- Add display mode toggles

### Phase 4 (Tool commands: /tools /toolsets /skills /plugins /cron)
- Add tool enable/disable at runtime
- Add toolset management
- Add skills hub listing

### Phase 5 (CLI entry points)
- Add hermes doctor, status, logs, setup, profiles, config, models, tools, backup, update
