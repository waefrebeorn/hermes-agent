# Hermes C Translation — State

## Phase 1: Foundation Dependencies ✅ (verified May 20)
| Dep | C File | Status | Verified |
|-----|--------|--------|----------|
| JSON parser | `src/deps/json.c` | ✅ | 5/5 smoke tests pass |
| YAML config parser | `src/deps/yaml.c` | ✅ | compiles |
| HTTP client (OpenSSL+sockets) | `src/deps/http.c` | ✅ | compiles |
| File-based session DB | `src/deps/db.c` | ✅ | compiles |
| Crypto (SHA-256/HMAC/base64) | `src/deps/crypto.c` | ✅ | compiles |
| Terminal display (ANSI) | `src/deps/cli_display.c` | ✅ | compiles |

## Phase 2: Agent Core ✅ (compiled + linked, verified May 20)
| File | Status | Notes |
|------|--------|-------|
| `src/agent/agent_loop.c` | ✅ | Core loop, tool dispatch scaffold |
| `src/agent/llm_client.c` | ✅ | OpenAI-compatible chat completions |
| `src/agent/context.c` | ✅ | Message array, system prompt, truncation |
| `src/agent/title.c` | ✅ | Extractive title gen (first 6 words) |
| `src/cli/config.c` | ✅ | YAML + .env config loader |
| `src/cli/cli.c` | ✅ | Interactive + one-shot CLI |
| `src/cli/display.c` | ✅ | Higher-level display wrappers |
| `src/cli/commands.c` | ✅ | Slash command registry |
| `src/cli/main.c` | ✅ | CLI entry + gateway stub |
| `src/main.c` | ✅ | Global entry point |
| Binary (`hermes`) | ✅ | Links + runs, `--version` prints banner |

## Phase 3: Tools — not started
| File | Status |
|------|--------|
| `src/tools/registry.c` | 🔲 |
| `src/tools/terminal.c` | 🔲 |
| `src/tools/file.c` | 🔲 |
| `src/tools/web.c` | 🔲 |
| `src/tools/skills.c` | 🔲 |

## Phase 4: Gateway — not started
## Phase 5: Cron + Advanced — not started

## Build
- compiler: gcc ✓
- deps: OpenSSL ✓ (static link), no other system libs
- phase 1: 6 objects, zero errors
- phase 2: 16 objects (including main.o), zero errors, links clean
- binary: `hermes` — version banner, CLI dispatch, one-shot mode
