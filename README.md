# Slermes C

**Location:** `~/hermes-agent-dev/`  
**Tracks:** `wubu/main` (waefrebeorn/slermes fork)  
**Purpose:** Development + C translation (Slermes)

This is the development workspace for WuBu Hermes — a fork of NousResearch/hermes-agent
with the long-term goal of translating the entire agent into C for zero-dependency operation.

## Fork Structure

```
waefrebeorn/slermes  (GitHub)
  ├── wubu main                  ← All our commits + upstream. Default branch.
  ├── feat/c-translation         ← C/ directory translation work
  ├── feat/some-feature          ← Individual feature branches (for PRs)
  └── fix/some-bug               ← Individual fix branches (for PRs)
```

## Remotes

| Remote | URL | Direction |
|--------|-----|-----------|
| `origin` | `https://github.com/NousResearch/hermes-agent.git` | Pull upstream updates |
| `wubu` | `git@github.com:waefrebeorn/slermes.git` | Push our work |

## Workflow

### Daily Pull (both Python + C updates)

```bash
git pull wubu main          # Get latest from our fork
python3 C/digest.py          # Check what C work is needed
```

### Making Changes

```bash
git checkout -b feat/my-thing wubu/main   # Branch from our main
# ... make changes (Python + C/ together) ...
git add -A
git commit -m "feat(x): description"
git push wubu feat/my-thing               # Push to our fork
# Open PR on GitHub: feat/my-thing → wubu/main
```

### Upstream Sync

```bash
git fetch origin main                     # Get latest upstream
git merge origin/main                     # Merge into current branch
python3 C/digest.py                       # Run digestion on changes
# Fix any C translation gaps from new upstream code
git push wubu HEAD                        # Push merged result
```

### C Translation

<<<<<<< HEAD
All C work lives in `C/`. See `C/README.md` for full details.
=======
---

## Skip the API-key collection — Nous Portal

Hermes works with whatever provider you want — that's not changing. But if you'd rather not collect five separate API keys for the model, web search, image generation, TTS, and a cloud browser, **[Nous Portal](https://portal.nousresearch.com)** covers all of them under one subscription:

- **300+ models** — pick any of them with `/model <name>`
- **Tool Gateway** — web search (Firecrawl), image generation (FAL), text-to-speech (OpenAI), cloud browser (Browser Use), all routed through your sub. No extra accounts.

One command from a fresh install:

```bash
hermes setup --portal
```

That logs you in via OAuth, sets Nous as your provider, and turns on the Tool Gateway. Check what's wired up any time with `hermes portal status`. Full details on the [Tool Gateway docs page](https://hermes-agent.nousresearch.com/docs/user-guide/features/tool-gateway).

You can still bring your own keys per-tool whenever you want — the gateway is per-backend, not all-or-nothing.

---

## CLI vs Messaging Quick Reference

Hermes has two entry points: start the terminal UI with `hermes`, or run the gateway and talk to it from Telegram, Discord, Slack, WhatsApp, Signal, or Email. Once you're in a conversation, many slash commands are shared across both interfaces.

| Action | CLI | Messaging platforms |
|---------|-----|---------------------|
| Start chatting | `hermes` | Run `hermes gateway setup` + `hermes gateway start`, then send the bot a message |
| Start fresh conversation | `/new` or `/reset` | `/new` or `/reset` |
| Change model | `/model [provider:model]` | `/model [provider:model]` |
| Set a personality | `/personality [name]` | `/personality [name]` |
| Retry or undo the last turn | `/retry`, `/undo` | `/retry`, `/undo` |
| Compress context / check usage | `/compress`, `/usage`, `/insights [--days N]` | `/compress`, `/usage`, `/insights [days]` |
| Browse skills | `/skills` or `/<skill-name>` | `/<skill-name>` |
| Interrupt current work | `Ctrl+C` or send a new message | `/stop` or send a new message |
| Platform-specific status | `/platforms` | `/status`, `/sethome` |

For the full command lists, see the [CLI guide](https://hermes-agent.nousresearch.com/docs/user-guide/cli) and the [Messaging Gateway guide](https://hermes-agent.nousresearch.com/docs/user-guide/messaging).

---

## Documentation

All documentation lives at **[hermes-agent.nousresearch.com/docs](https://hermes-agent.nousresearch.com/docs/)**:

| Section | What's Covered |
|---------|---------------|
| [Quickstart](https://hermes-agent.nousresearch.com/docs/getting-started/quickstart) | Install → setup → first conversation in 2 minutes |
| [CLI Usage](https://hermes-agent.nousresearch.com/docs/user-guide/cli) | Commands, keybindings, personalities, sessions |
| [Configuration](https://hermes-agent.nousresearch.com/docs/user-guide/configuration) | Config file, providers, models, all options |
| [Messaging Gateway](https://hermes-agent.nousresearch.com/docs/user-guide/messaging) | Telegram, Discord, Slack, WhatsApp, Signal, Home Assistant |
| [Security](https://hermes-agent.nousresearch.com/docs/user-guide/security) | Command approval, DM pairing, container isolation |
| [Tools & Toolsets](https://hermes-agent.nousresearch.com/docs/user-guide/features/tools) | 40+ tools, toolset system, terminal backends |
| [Skills System](https://hermes-agent.nousresearch.com/docs/user-guide/features/skills) | Procedural memory, Skills Hub, creating skills |
| [Memory](https://hermes-agent.nousresearch.com/docs/user-guide/features/memory) | Persistent memory, user profiles, best practices |
| [MCP Integration](https://hermes-agent.nousresearch.com/docs/user-guide/features/mcp) | Connect any MCP server for extended capabilities |
| [Cron Scheduling](https://hermes-agent.nousresearch.com/docs/user-guide/features/cron) | Scheduled tasks with platform delivery |
| [Context Files](https://hermes-agent.nousresearch.com/docs/user-guide/features/context-files) | Project context that shapes every conversation |
| [Architecture](https://hermes-agent.nousresearch.com/docs/developer-guide/architecture) | Project structure, agent loop, key classes |
| [Contributing](https://hermes-agent.nousresearch.com/docs/developer-guide/contributing) | Development setup, PR process, code style |
| [CLI Reference](https://hermes-agent.nousresearch.com/docs/reference/cli-commands) | All commands and flags |
| [Environment Variables](https://hermes-agent.nousresearch.com/docs/reference/environment-variables) | Complete env var reference |

---

## Migrating from OpenClaw

If you're coming from OpenClaw, Hermes can automatically import your settings, memories, skills, and API keys.

**During first-time setup:** The setup wizard (`hermes setup`) automatically detects `~/.openclaw` and offers to migrate before configuration begins.

**Anytime after install:**
>>>>>>> upstream/main

```bash
# Build current C phase
make -C C phase3

# Full build
make -C C

# Run digestion after any git pull
python3 C/digest.py
```

## Key Directories

| Path | Purpose |
|------|---------|
| `C/` | C translation (parallel implementation) |
| `skills/` | Custom WuBu Hermes skills (mind-palace, session-goal-paste, etc.) |
| `tools/` | Tool implementations (security-hardened) |
| `tests/tools/test_path_security.py` | Security test suite (49 tests) |

## Unique Commits (14 on wubu/main)

```
60a91505f  fix(test_openclaw): add pathlib to KNOWN_FALSE_POSITIVES
fed55c6c9  fix(tests): align drain resume_pending tests
9895657ce  test: add path_security test suite (49 tests)
d316ea08a  fix: V4A traversal bypass, regex gaps, search_tool guard
e3ec8a1a1  fix(security): remove leading space in setuid/setgid
7720f7df6  fix(security): fix backtick_subshell regex
4b14fd684  fix(security): add path traversal guards
cbc065880  fix(ci): guard nix-lockfile-fix workflow for upstream
01ad48b0c  fix: resolve npm vulnerabilities
1eb529ce4  Add session-goal-paste skill: templates
b06ae4ed3  Add optimizer-research-2026 skill
71557b4e3  Add session-goal-paste skill
e5171ff4c  Add mind-palace skill
d9fab3636  fix: skip auto-title for local endpoints
```
