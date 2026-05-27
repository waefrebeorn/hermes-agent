# Slermes C — State Dashboard (v65 — 2026-05-27)

## Build Metrics
Build clean. **84 unique tools** (registry_register, +1 video_analyze). 98 CLI commands (COMMANDS[] table). 19 gateways. 10 provider types + metadata utility. 59 libs. 146 src/ .c files (non-deps). 226 test_*.c files. Binary: 30M. Suite: 258/0/0.

## 1:1 Parity Status (Triple DA v16)
~297 item-level gaps (battleship-v16 rows, 26 stale 1B claims removed).

## Recent (this session)
- vision: added OCR text extraction via tesseract (analysis="ocr"). Calls Python helper to run tesseract binary, returns extracted text and character count. Closes 1 vision-tool depth gap (OCR).
- video_analyze: new video analysis tool using ffprobe. Returns video metadata (codec, resolution, duration, bitrate, framerate), audio stream info, and scene detection via ffprobe lavfi. Supports local files and remote URLs. Closes 1 Phase 4 missing-tool gap (video_analyze).
- video_analyze tests: added 3 functional tests for ffprobe availability, JSON output parsing, and schema handling. Suite: 258/0/0.
- image_gen: added output_format param for controlling image output format (png, jpeg, webp). Closes 1 tool-depth gap (output_format).
- web: added SSRF protection via url_is_safe() check before HTTP requests. Blocks internal/private addresses. Closes 1 web-tool depth gap (request SSRF protection).
- image_gen: added save_local param to control whether generated image is saved to local file. Set false to return URL only (faster, no file I/O). Closes 1 tool-depth gap (save_local).
- x_search: added exclude_retweets param to filter retweets from search results. Closes 1 x_search depth gap (exclude_retweets).
- web: added include_body param for skipping response body in output. Set false to get URL + status code only. Closes 1 web-tool depth gap (body-less request).
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
3. Tool Features — ALL DONE
4. Missing Tools (45)
5-11: Gateway (51) → Agent (74) → Plugins (13) → Libs (19) → Security (15) → Tests (50) → Infra (10)
