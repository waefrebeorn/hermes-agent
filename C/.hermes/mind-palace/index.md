# Hermes C — Index (v9 — 2026-06-01)

## Walkway (read order for new session bot)
| File | Purpose |
|------|---------|
| **prestige_prompt.md** | Full resume + priority queue + bugfix history |
| **goal-mantra.md** | Single pasteable session start block (~35 lines) |
| **state.md** | Binary truth table, verified components, session log |
| **plan.md** | Phase roadmap, next tasks |
| **entry.md** | Build/run commands + architecture diagram |
| **overnight-map.md** | Autonomous session navigation + fallback |

## Plans Directory
| File | Purpose |
|------|---------|
| `plans/300-gap-roadmap-v1.md` | **Active** — 16-sector Python→C gap analysis with battleship IDs (A01-R18). 447 total gaps, 161 complete |
| `plans/current-state-audit.md` | Legacy state audit (stale — use state.md) |
| `plans/cli-1to1-parity.md` | Legacy CLI parity (stale — use 300-gap-roadmap) |
| `plans/tool-parity.md` | Legacy tool parity (stale — use 300-gap-roadmap) |
| `plans/gateway-platform-parity.md` | Legacy gateway parity (stale — use 300-gap-roadmap) |
| `plans/devils_advocate_v6.md` | Last full DA audit |
| `plans/*.md` (other) | Historical — kept for reference, not updated |

## Vault
| File | Purpose |
|------|---------|
| `vault/achievements.md` | All milestones, bug bounties, subsystem parity (up to date) |
| `vault/translation-essay.md` | Philosophical essay on the translation (5/27 with June 2026 update) |
| `vault/translation-essay-2.md` | "The Gap Reveal" — honest accounting sequel (6/1) |

## Root-Level Docs
| File | Purpose |
|------|---------|
| `README.md` | Project dashboard + quick start (up to date) |
| `ARCHITECTURE.md` | System architecture diagram + data flow |
| `CHANGELOG.md` | Version history from v0.1.0 |
| `SECURITY.md` | Disclosure policy + security audit |
| `DEPENDENCIES.md` | Third-party dependency list |
| `digestion.md` | Digest.py tool documentation |

## Caveats
- Some plan files in `plans/` are from May 2026 and reference old gap counts (117/0/0 suite, 81 test files). Cross-check against `state.md` for current numbers.
- `project.md` was merged into `goal-mantra.md` — use that instead.
- `testing.md` — use `test_runner.sh` and `state.md` suite results instead.
