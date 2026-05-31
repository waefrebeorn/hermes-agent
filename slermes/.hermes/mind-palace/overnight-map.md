# Overnight Map — Recent Phases (v446)

| Phase | Sector | Focus | From | To |
|-------|--------|-------|------|----|
| 390 | S7 X04 | Tool Coercion Number Edge Cases — 6 new assertions (36→42 total). Just minus/plus/decimal/minus-decimal all rejected. Leading zeros (00042→42) accepted. NULL output/is_int pointer safety. | Suite 338/0/13, 53 gaps, v445 | Suite 338/0/13, 53 gaps, v446 |
| 389 | S7 X04 | Process Tool Edge Case Expansion — 8 new assertions (26→34 total). Negative session_id (poll: error). Empty command (no crash). Write/close stdin to cat process (write returns written, close no crash). Signal by number 15/SIGTERM (signal_sent). Log non-existent session (error). | Suite 338/0/13, 53 gaps, v444 | Suite 338/0/13, 53 gaps, v445 |
