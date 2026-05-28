/*
 * datetime.h — C datetime library (J05: Python datetime port).
 *
 * Public API for timestamp operations: ISO 8601 parsing/formatting,
 * relative time descriptions, and date math.
 * All returned strings are malloc'd — caller must free().
 */

#ifndef HERMES_DATETIME_H
#define HERMES_DATETIME_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DATETIME_ISO8601_LEN 32  /* max ISO 8601 buffer size */

/* ─── Constructors ────────────────────────────────────────── */

/** datetime_now() — Current time as ISO 8601 string. Caller free(). */
char *datetime_now(void);

/** datetime_from_time_t(ts) — Convert time_t to ISO 8601 string (local). Caller free(). */
char *datetime_from_time_t(time_t ts);

/** datetime_from_time_t_utc(ts) — Convert time_t to ISO 8601 string (UTC). Caller free(). */
char *datetime_from_time_t_utc(time_t ts);

/** datetime_parse_iso8601(str) — Parse ISO 8601 to time_t. Returns -1 on error. */
time_t datetime_parse_iso8601(const char *str);

/** datetime_parse_rfc3339(str) — Parse RFC 3339 to time_t (alias, same as iso8601). Returns -1 on error. */
time_t datetime_parse_rfc3339(const char *str);

/** datetime_now_ts() — Current time as time_t. */
time_t datetime_now_ts(void);

/* ─── Formatting ──────────────────────────────────────────── */

/** datetime_format(ts, fmt) — Format time_t with strftime format. Caller free().
 *  Returns NULL on error. Example: datetime_format(ts, "%Y-%m-%d %H:%M:%S") */
char *datetime_format(time_t ts, const char *fmt);

/** datetime_format_utc(ts, fmt) — Same but in UTC. Caller free(). */
char *datetime_format_utc(time_t ts, const char *fmt);

/** datetime_format_tz(ts, tz_name, fmt) — Format time_t in named timezone.
 *  tz_name: e.g. "America/New_York", "Europe/London", "Asia/Tokyo".
 *  Returns NULL on error. Caller free(). */
char *datetime_format_tz(time_t ts, const char *tz_name, const char *fmt);

/* ─── Relative time ───────────────────────────────────────── */

/** datetime_describe(ts) — Relative description of timestamp vs now.
 *  E.g. "just now", "2 minutes ago", "3 hours ago", "5 days ago",
 *  "2026-05-22" (if > 7 days). Caller free(). */
char *datetime_describe(time_t ts);

/** datetime_age_seconds(ts) — Seconds since ts (negative if in future). */
double datetime_age_seconds(time_t ts);

/** datetime_age_minutes(ts) — Minutes since ts. */
double datetime_age_minutes(time_t ts);

/** datetime_age_hours(ts) — Hours since ts. */
double datetime_age_hours(time_t ts);

/** datetime_age_days(ts) — Days since ts. */
double datetime_age_days(time_t ts);

/* ─── Date math ───────────────────────────────────────────── */

/** datetime_add_days(ts, days) — Add (or subtract) days from timestamp. */
time_t datetime_add_days(time_t ts, int days);

/** datetime_add_hours(ts, hours) — Add (or subtract) hours from timestamp. */
time_t datetime_add_hours(time_t ts, int hours);

/** datetime_add_seconds(ts, secs) — Add (or subtract) seconds from timestamp. */
time_t datetime_add_seconds(time_t ts, int secs);

/** datetime_is_expired(ts, ttl_seconds) — True if ts + ttl_seconds < now. */
bool datetime_is_expired(time_t ts, int ttl_seconds);

/* ─── Queries ─────────────────────────────────────────────── */

/** datetime_is_today(ts) — True if ts is on today's calendar date (local tz). */
bool datetime_is_today(time_t ts);

/** datetime_is_yesterday(ts) — True if ts is on yesterday's calendar date. */
bool datetime_is_yesterday(time_t ts);

/** datetime_same_day(a, b) — True if a and b fall on same calendar day. */
bool datetime_same_day(time_t a, time_t b);

/** datetime_day_start(ts) — Midnight (00:00:00) of ts's calendar day. */
time_t datetime_day_start(time_t ts);

/* ─── Timezone ───────────────────────────────────────────── */

/** datetime_localtime_offset(void) — Local timezone offset from UTC in seconds.
 *  Positive east of UTC (e.g. UTC+2 = +7200). Returns 0 on error. */
int datetime_localtime_offset(void);

/** datetime_tz_offset(tz_name) — Get named timezone offset from UTC in seconds.
 *  tz_name: e.g. "America/New_York". Returns 0 on error. */
int datetime_tz_offset(const char *tz_name);

/* ─── Utilities ───────────────────────────────────────────── */

/** datetime_buffer(time_t ts, char *buf, size_t cap) — Write ISO 8601 to buffer.
 *  Same as datetime_from_time_t but no alloc. Returns buf on success, NULL on error. */
char *datetime_buffer(time_t ts, char *buf, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_DATETIME_H */
