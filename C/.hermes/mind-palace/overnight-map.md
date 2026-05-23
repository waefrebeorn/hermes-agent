# Overnight Map — 2026-05-27

## Active Session: J05 datetime Library ✅ CLOSED

**Libs: 35% → 38%** (1/12 remaining gaps closed: J05)
**Suite: 116/0/0** (+1 test file, 59 assertions)

## What Was Done

- New lib/libdatetime/datetime.h + datetime.c: 20 public functions
- Full ISO 8601 parsing (T/Z/space/offset/date-only), formatting, relative time, date math
- tests/test_datetime.c — 59 assertions
- Wired into Makefile + test_runner.sh

## Remaining Libs gaps

- 12 Python-equivalent library ports remaining (J06-J17)

## P1 Gap Options

1. **J06: libcsv** — CSV parsing (Python csv module) — used for data export
2. **B34-B36: Remaining provider-specific API quirks** — check which are open
3. **M32-M44: Per-tool tests** — browser, computer_use, file_batch, homeassistant, image_gen, result_storage, session_crud, session_search, voice_mode, x_search
4. **Plugin depth** — 48 gaps, still 14%
5. **O02: Windows build** — last build/doc gap
