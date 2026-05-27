# Slermes C — State Dashboard (v60 — 2026-05-27)

## Build Metrics
Build clean. **83 unique tools** (registry_register). 98 CLI commands (COMMANDS[] table). 19 gateways. 10 provider types + metadata utility. 59 libs. 146 src/ .c files (non-deps). 224 test_*.c files. Binary: 30M. Suite: 257/0/0.

## 1:1 Parity Status (Triple DA v16)
~301 item-level gaps (battleship-v16 rows, 26 stale 1B claims removed).

## Recent (this session)
- image_gen: added output_format param for controlling image output format (png, jpeg, webp). Closes 1 tool-depth gap (output_format).
- web: added SSRF protection via url_is_safe() check before HTTP requests. Blocks internal/private addresses. Closes 1 web-tool depth gap (request SSRF protection).
- image_gen: added save_local param to control whether generated image is saved to local file. Set false to return URL only (faster, no file I/O). Closes 1 tool-depth gap (save_local).
- x_search: added sort_order param (relevance/recency) for controlling search result ordering. Closes 1 x_search depth gap (sort order).
- image_gen: added image_url param for image-to-image generation (img2img). Pass a reference image URL to use as source for generation. Closes 1 tool-depth gap (img2img).
- vision: added analysis param with color palette (dominant colors via PIL) and EXIF extraction modes. Closes 2 vision-tool gaps (color analysis, EXIF).
- send_message: added reply_to_message_id param for replying to specific messages. Closes 1 tool-depth gap (reply_to).
- image_gen: added seed + num_images params for reproducibility and multi-image generation. Closes 2 tool-depth gaps (seed param, multi-image).
- file_batch: added batch rename action. POSIX glob-based pattern matching with wildcard extraction/substitution. Supports "pattern" + "dest_pattern" params (e.g. pattern="*.txt" + dest_pattern="backup_*.md"). Falls back to files+dest for simple rename. Closes 1 tool-depth gap (batch rename).
- file_batch: added hash action (SHA-256 via libhash). Reads file, returns hex sha256. 100MB cap. Closes 1 tool-depth gap (batch hash).
- terminal.c: fixed JSON schema bug - stray escaped quote after `modal` in backend description. Schema parser was lenient but would reject at strict parse. Removed extra quote between `}` and `,`.
- file_batch: added batch chmod action (`mode` param, octal e.g. '755'). Parse mode string -> mode_t, sandbox-checked per file, per-file result reporting. Closes 1 tool-depth gap for file_operations (batch chmod/chown).
- file_batch: added batch touch action. Creates file if missing, updates timestamp if exists. Uses utimensat() + fopen() fallback. Closes 1 tool-depth gap (batch touch).

## Phase Order
0. Display Parity (16) — 14/16 done
1. CLI Args — ALL DONE
2. Provider Parity (~20 real)
3. Tool Features (3)
4. Missing Tools (46)
5-11: Gateway (51) → Agent (74) → Plugins (13) → Libs (19) → Security (15) → Tests (50) → Infra (10)
