# Hermes C — Tool Parity Spec (May 21 AM)

## Current: 27 built-in tools = 18 core + 4 browser + 1 approval + provider framework
## Target: 40+ tools matching Python

## 1. EXISTING TOOLS — Quality Audit

| Tool | File | Lines | Status | Issues |
|------|------|-------|--------|--------|
| terminal | terminal.c | ~200 | ✅ | No docker/ssh backend |
| file | file.c | ~300 | ✅ | No checkpoint/rollback |
| web | web.c | ~200 | ✅ | No URL safety validation |
| skills | skills.c | ~150 | ✅ | No skills hub/sync |
| patch | patch.c | ~200 | ✅ | Works |
| exec_code | exec_code.c | ~200 | ✅ | No sandbox isolation |
| clarify | clarify.c | ~150 | ✅ | Works |
| memory | memory.c | ~200 | ✅ | No versioning |
| todo | todo.c | ~200 | ✅ | No sync |
| process | process.c | ~300 | ✅ | No multi-backend |
| send_message | send_message.c | ~250 | ✅ | No platform routing |
| cronjob | cronjob.c | ~200 | ✅ | No cron UI |
| skill_mgmt | skill_mgmt.c | ~200 | ✅ | No hub/sync |
| session_search | session_search.c | ~200 | ✅ | Basic FTS |
| tts | tts.c | ~150 | ✅ | Single provider |
| vision | vision.c | ~200 | ✅ | Single provider |
| delegate | delegate.c | ~200 | ✅ | Basic |
| x_search | x_search.c | ~200 | ✅ | Works |
| browser_navigate | browser.c | ~300 | ✅ | New — navigate to URL |
| browser_snapshot | browser.c | ~400 | ✅ | New — page text |
| browser_back | browser.c | ~150 | ✅ | New — history back |
| browser_forward | browser.c | ~150 | ✅ | New — history forward |
| approval_status | approval.c | ~200 | ✅ | New — security status |
| provider framework | provider.c/.h | ~800 | ✅ | New — abstract interface |

## 2. MISSING TOOLS — P0 Priority (block basic usage)

### Browser Automation (12 tools) — ~3,000 LOC
| Tool | Python Ref | Description | Depends On |
|------|-----------|-------------|------------|
| browser_navigate | browser_tool.py | Navigate to URL, wait for load | http_client |
| browser_snapshot | browser_tool.py | Capture page state (DOM, screenshot) | html_parser |
| browser_click | browser_tool.py | Click element by selector | browser_state |
| browser_type | browser_tool.py | Type text into element | browser_state |
| browser_scroll | browser_tool.py | Scroll page | browser_state |
| browser_back | browser_tool.py | Navigate back | browser_state |
| browser_press | browser_tool.py | Press keyboard key | browser_state |
| browser_get_images | browser_tool.py | Extract images from page | html_parser |
| browser_vision | browser_tool.py | Vision on page elements | vision_tool |
| browser_console | browser_tool.py | Execute JS, get console logs | js_engine |
| browser_cdp | browser_tool.py | Chrome DevTools Protocol | websocket |
| browser_dialog | browser_tool.py | Handle alert/confirm dialogs | browser_state |

Implementation approach: C port of Playwright/CDP via WebSocket. Each tool is a thin wrapper over connection state.

### Security Tools (5 tools) — ~1,500 LOC
| Tool | Python Ref | Description |
|------|-----------|-------------|
| approval | approval.py (1393 LOC) | Dangerous command approval workflow |
| path_security | path_security.py | Path traversal protection |
| url_safety | url_safety.py (351 LOC) | SSRF protection |
| tirith_security | tirith_security.py (803 LOC) | Pre-exec security scanning |
| credential_files | credential_files.py (436 LOC) | Secure credential storage |

### Image Generation (1 tool) — ~500 LOC
| Tool | Python Ref | Description |
|------|-----------|-------------|
| image_generate | image_gen/ | Multi-provider image gen |

## 3. MISSING TOOLS — P1 Priority (unlocks capability)

### Kanban (8 tools) — ~2,000 LOC
| Tool | Description |
|------|-------------|
| kanban_create_board | Create board |
| kanban_board | Get board state |
| kanban_create_lane | Create lane |
| kanban_create_card | Create card |
| kanban_move_card | Move card between lanes |
| kanban_update_card | Update card properties |
| kanban_assign | Assign card to user |
| kanban_archive | Archive completed cards |

### Spotify (7 tools) — ~1,000 LOC
| Tool | Description |
|------|-------------|
| spotify_play | Play/resume |
| spotify_pause | Pause |
| spotify_search | Search tracks |
| spotify_queue | Queue track |
| spotify_next | Next track |
| spotify_previous | Previous track |
| spotify_current | Current playback state |

### Other P1 Tools
| Tool | Description |
|------|-------------|
| home_assistant_* (4 tools) | Smart home control |
| feishu_doc_read/write | Feishu document tools |
| transcription | Audio transcription (Whisper) |
| mcp_tool | MCP client integration |
| checkpoint_manager (1638 LOC) | Filesystem snapshots |
| skills_hub | Community skill sharing |
| terminal docker/ssh backends | Remote execution |

## 4. EXISTING TOOL QUALITY UPGRADES

| Tool | Upgrade | Priority |
|------|---------|----------|
| terminal | Add docker, ssh, modal backends | P1 |
| terminal | Add dangerous command approval integration | P0 |
| file | Add checkpoint/rollback manager | P1 |
| file | Add path traversal protection | P0 |
| web | Add URL safety validation | P0 |
| web | Add robots.txt compliance | P2 |
| exec_code | Add sandbox isolation | P1 |
| exec_code | Add OsV vulnerability check | P2 |
| memory | Add versioned memory | P2 |
| todo | Add sync/export | P2 |
| tts | Add multiple TTS providers | P1 |
| vision | Add multiple vision providers | P1 |
| delegate | Add multi-agent orchestration | P1 |
| session_search | Upgrade to full FTS5 | P1 |
| skills | Add skills hub/sync | P1 |
| skills | Add skills guard/provenance | P2 |
| send_message | Add platform routing | P1 |
| process | Add multi-backend | P1 |

## 5. Implementation Order
### Phase 1: Security (P0 — cannot ship without)
1. approval.c (~500 LOC) — dangerous command approval ✅ DONE
2. path_security.c (~200 LOC) — path traversal protection
3. url_safety.c (~300 LOC) — SSRF protection

### Phase 2: Browser (P0 — core capability)
4. browser_navigate.c (~300 LOC) ✅ DONE
5. browser_snapshot.c (~400 LOC) ✅ DONE
6. browser_click.c (~200 LOC)
7. browser_type.c (~200 LOC)
8. browser_scroll.c (~150 LOC)
9. browser_back.c (~100 LOC) ✅ DONE

### Phase 3: Image Gen (P0 — core capability)
10. image_generate.c (~500 LOC)

### Phase 4: Advanced Tools (P1)
11-18. kanban tools (~2,000 LOC)
19-25. spotify tools (~1,000 LOC)
26. transcription.c (~500 LOC)
27. mcp_tool.c (~500 LOC)
28-31. home_assistant tools (~500 LOC)
