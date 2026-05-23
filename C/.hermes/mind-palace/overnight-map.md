# Overnight Map — 2026-05-23 (DA v25)

## Active: mcp_oauth + mcp_oauth_manager ported. 193/500 parity.

**Suite: test binary 58/58 ✅, make builds clean**
**Binary: builds clean (~10MB dynamic)**
**Commits: `58d71c10b` (D85), `80192742e` (D86) — both pushed to wubu remote**

## What Was Done (May 23)
- **mcp_oauth (D85):** Port of Python `tools/mcp_oauth.py` (648 lines → ~900 C).
  Token storage (JSON, 0o600 perms, atomic write), PKCE (SHA256+base64url),
  callback HTTP server, metadata discovery (well-known endpoint),
  token exchange (authorization_code grant), token refresh (refresh_token grant),
  browser open (xdg-open/open/gnome-open/kde-open),
  `build_oauth_auth()` high-level entry point.
- **mcp_oauth_manager (D86):** Port of Python `tools/mcp_oauth_manager.py` (607 lines → ~300 C).
  Per-server registry (32 max), mtime-based disk-change detection,
  `manager_get_token()` (mtime→cached→refresh→full flow),
  `manager_reauthorize()` (clear+re-auth).

## P0 Gaps (next session picks first)
1. **D80**: fal_common.py — Fal shared utilities (163 lines)
2. **S03**: computer_use Wayland fallback
3. **S08-S09**: image_gen local provider + caching
4. **D75-D79**: computer_use upstream Python backports (already ✅ from earlier)
5. **MCP sector (F)**: remaining 9 gaps in MCP/transports
