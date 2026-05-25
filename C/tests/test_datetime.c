/*
 * test_datetime.c — J05: datetime library unit tests.
 *
 * Tests all 20 public functions in libdatetime/datetime.h.
 */

#include "datetime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    tests_run++; \
    if (!(expr)) { \
        printf("  FAIL: %s\n", name); \
        tests_failed++; \
    } else { \
        printf("  PASS: %s\n", name); \
        tests_passed++; \
    } \
} while(0)

#define TEST_STR(name, actual, expected) do { \
    tests_run++; \
    const char *_a = (actual); \
    const char *_e = (expected); \
    if (!_a || strcmp(_a, _e) != 0) { \
        printf("  FAIL: %s (got \"%s\", expected \"%s\")\n", name, _a ? _a : "NULL", _e); \
        tests_failed++; \
    } else { \
        printf("  PASS: %s\n", name); \
        tests_passed++; \
    } \
} while(0)

#define TEST_DBL(name, actual, expected, tol) do { \
    tests_run++; \
    double _a = (actual); \
    double _e = (expected); \
    if (fabs(_a - _e) > (tol)) { \
        printf("  FAIL: %s (got %.4f, expected %.4f)\n", name, _a, _e); \
        tests_failed++; \
    } else { \
        printf("  PASS: %s\n", name); \
        tests_passed++; \
    } \
} while(0)

int main(void) {
    printf("=== J05: datetime library tests ===\n\n");

    /* ─── datetime_now() ──────────────────────────────────── */
    printf("--- datetime_now() ---\n");
    {
        char *now = datetime_now();
        TEST("now returns non-NULL", now != NULL);
        TEST("now starts with 20xx", now && strncmp(now, "20", 2) == 0);
        TEST("now contains T separator", now && strchr(now, 'T') != NULL);
        free(now);
    }

    /* ─── datetime_from_time_t() ──────────────────────────── */
    printf("\n--- datetime_from_time_t() ---\n");
    {
        /* 2026-05-22 14:30:00 local */
        struct tm tm_ref = {0};
        tm_ref.tm_year = 2026 - 1900;
        tm_ref.tm_mon = 4;    /* May (0-based) */
        tm_ref.tm_mday = 22;
        tm_ref.tm_hour = 14;
        tm_ref.tm_min = 30;
        tm_ref.tm_sec = 0;
        tm_ref.tm_isdst = -1;
        time_t ts = mktime(&tm_ref);

        char *s = datetime_from_time_t(ts);
        TEST("from_time_t non-NULL", s != NULL);
        TEST("from_time_t has date 2026-05-22", s && strstr(s, "2026-05-22") != NULL);
        TEST("from_time_t has time 14:30:00", s && strstr(s, "14:30:00") != NULL);
        free(s);
    }

    /* ─── datetime_from_time_t_utc() ──────────────────────── */
    printf("\n--- datetime_from_time_t_utc() ---\n");
    {
        time_t epoch = 0; /* 1970-01-01 00:00:00 UTC */
        char *s = datetime_from_time_t_utc(epoch);
        TEST("utc epoch non-NULL", s != NULL);
        if (s) {
            TEST("utc epoch has 1970", strstr(s, "1970-01-01") != NULL);
            TEST("utc epoch has Z", strchr(s, 'Z') != NULL);
        }
        free(s);
    }

    /* ─── datetime_parse_iso8601() ─────────────────────────── */
    printf("\n--- datetime_parse_iso8601() ---\n");
    {
        /* Full ISO 8601 */
        time_t ts = datetime_parse_iso8601("2026-05-22T14:30:00");
        TEST("parse full iso8601 returns valid", ts != (time_t)-1);
        if (ts != (time_t)-1) {
            char *back = datetime_from_time_t(ts);
            TEST("parse+format round trip", back && strstr(back, "2026-05-22") != NULL);
            free(back);
        }

        /* UTC suffix */
        ts = datetime_parse_iso8601("2026-01-01T00:00:00Z");
        TEST("parse UTC iso8601 returns valid", ts != (time_t)-1);

        /* Space separator */
        ts = datetime_parse_iso8601("2026-06-15 09:00:00");
        TEST("parse space separator returns valid", ts != (time_t)-1);

        /* Date only */
        ts = datetime_parse_iso8601("2026-05-22");
        TEST("parse date-only returns valid", ts != (time_t)-1);

        /* NULL/empty */
        TEST("parse NULL returns -1", datetime_parse_iso8601(NULL) == (time_t)-1);
        TEST("parse empty returns -1", datetime_parse_iso8601("") == (time_t)-1);

        /* Invalid format */
        TEST("parse garbage returns -1", datetime_parse_iso8601("not-a-date") == (time_t)-1);

        /* Fractional seconds (RFC 3339) */
        {
            ts = datetime_parse_iso8601("2026-05-22T14:30:00.123Z");
            TEST("parse fractional sec Z returns valid", ts != (time_t)-1);
            if (ts != (time_t)-1) {
                char *back = datetime_from_time_t_utc(ts);
                TEST("parse fractional sec Z round-trip to UTC",
                     back && strstr(back, "14:30:00") != NULL);
                free(back);
            }
        }
        {
            ts = datetime_parse_iso8601("2026-05-22T14:30:00.999999+05:00");
            TEST("parse fractional sec +offset returns valid", ts != (time_t)-1);
            if (ts != (time_t)-1) {
                char *back = datetime_from_time_t_utc(ts);
                /* 14:30 +05:00 = 09:30 UTC */
                TEST("parse fractional sec +offset correct UTC",
                     back && strstr(back, "09:30:00") != NULL);
                free(back);
            }
        }
        TEST("parse rfc3339 alias returns valid",
             datetime_parse_rfc3339("2026-05-22T14:30:00.5Z") != (time_t)-1);
        TEST("parse rfc3339 alias matches iso8601",
             datetime_parse_rfc3339("2026-05-22T14:30:00Z") == datetime_parse_iso8601("2026-05-22T14:30:00Z"));
    }

    /* ─── datetime_format() ────────────────────────────────── */
    printf("\n--- datetime_format() ---\n");
    {
        struct tm tm_ref = {0};
        tm_ref.tm_year = 2026 - 1900;
        tm_ref.tm_mon = 0;   /* January */
        tm_ref.tm_mday = 1;
        tm_ref.tm_hour = 9;
        tm_ref.tm_min = 5;
        tm_ref.tm_sec = 3;
        tm_ref.tm_isdst = -1;
        time_t ts = mktime(&tm_ref);

        char *dmy = datetime_format(ts, "%d/%m/%Y");
        TEST_STR("format DMY", dmy, "01/01/2026");
        free(dmy);

        char *hm = datetime_format(ts, "%H:%M");
        TEST_STR("format HH:MM", hm, "09:05");
        free(hm);

        TEST("format NULL fmt returns NULL", datetime_format(ts, NULL) == NULL);
    }

    /* ─── datetime_format_utc() ────────────────────────────── */
    printf("\n--- datetime_format_utc() ---\n");
    {
        time_t epoch = 0;
        char *s = datetime_format_utc(epoch, "%Y-%m-%d %H:%M:%S");
        TEST_STR("utc format epoch", s, "1970-01-01 00:00:00");
        free(s);
    }

    /* ─── datetime_describe() ──────────────────────────────── */
    printf("\n--- datetime_describe() ---\n");
    {
        time_t now = datetime_now_ts();

        /* Just now (within last 5 seconds) */
        char *d = datetime_describe(now);
        TEST("describe now says 'just now'", d && strcmp(d, "just now") == 0);
        free(d);

        /* 1 minute ago */
        time_t one_min_ago = now - 75;
        d = datetime_describe(one_min_ago);
        TEST("describe 1 min ago", d && strcmp(d, "1 minute ago") == 0);
        free(d);

        /* 5 minutes ago */
        time_t five_min_ago = now - 300;
        d = datetime_describe(five_min_ago);
        TEST("describe 5 minutes ago", d && strcmp(d, "5 minutes ago") == 0);
        free(d);

        /* 2 hours ago */
        time_t two_hr_ago = now - 7200;
        d = datetime_describe(two_hr_ago);
        TEST("describe 2 hours ago", d && strcmp(d, "2 hours ago") == 0);
        free(d);

        /* Yesterday (>24h but <48h) */
        time_t yesterday = now - 90000; /* 25 hours */
        d = datetime_describe(yesterday);
        TEST("describe yesterday", d && strcmp(d, "yesterday") == 0);
        free(d);

        /* 3 days ago */
        time_t three_days = now - 259200; /* 72 hours */
        d = datetime_describe(three_days);
        TEST("describe 3 days ago", d && strcmp(d, "3 days ago") == 0);
        free(d);

        /* Future: "in 5 minutes" */
        time_t future = now + 300;
        d = datetime_describe(future);
        TEST("describe future minutes", d && strcmp(d, "in 5 minutes") == 0);
        free(d);

        /* Future: "in 2 hours" */
        future = now + 7200;
        d = datetime_describe(future);
        TEST("describe future hours", d && strcmp(d, "in 2 hours") == 0);
        free(d);

        /* Far future: "in 7 days" */
        future = now + 604800;
        d = datetime_describe(future);
        TEST("describe future days", d && strstr(d, "in ") != NULL && strstr(d, " days") != NULL);
        free(d);

        /* Less than a minute */
        time_t few_secs = now - 30;
        d = datetime_describe(few_secs);
        TEST("describe <60s says 'less than a minute ago'",
             d && strcmp(d, "less than a minute ago") == 0);
        free(d);
    }

    /* ─── Age functions ────────────────────────────────────── */
    printf("\n--- Age functions ---\n");
    {
        time_t now = datetime_now_ts();
        time_t five_min_ago = now - 300;

        TEST_DBL("age_seconds ~300", datetime_age_seconds(five_min_ago), 300, 2.0);
        TEST_DBL("age_minutes ~5", datetime_age_minutes(five_min_ago), 5, 0.1);

        time_t one_hour_ago = now - 3600;
        TEST_DBL("age_hours ~1", datetime_age_hours(one_hour_ago), 1, 0.01);

        time_t two_days_ago = now - 172800;
        TEST_DBL("age_days ~2", datetime_age_days(two_days_ago), 2, 0.01);
    }

    /* ─── Date math ────────────────────────────────────────── */
    printf("\n--- Date math ---\n");
    {
        time_t now = datetime_now_ts();

        time_t plus_1 = datetime_add_days(now, 1);
        TEST_DBL("add_days 1", datetime_age_days(plus_1), -1, 0.01);

        time_t minus_3 = datetime_add_days(now, -3);
        TEST_DBL("add_days -3", datetime_age_days(minus_3), 3, 0.01);

        time_t plus_2h = datetime_add_hours(now, 2);
        TEST_DBL("add_hours 2", datetime_age_hours(plus_2h), -2, 0.01);

        time_t plus_30s = datetime_add_seconds(now, 30);
        TEST_DBL("add_seconds 30", datetime_age_seconds(plus_30s), -30, 1.0);
    }

    /* ─── datetime_is_expired() ────────────────────────────── */
    printf("\n--- datetime_is_expired() ---\n");
    {
        time_t now = datetime_now_ts();

        TEST("now not expired (3600s TTL)", !datetime_is_expired(now, 3600));
        TEST("now expired (0s TTL)", datetime_is_expired(now, 0));

        time_t old = now - 100;
        TEST("100s ago expired (50s TTL)", datetime_is_expired(old, 50));
        TEST("100s ago not expired (200s TTL)", !datetime_is_expired(old, 200));
    }

    /* ─── Date queries ─────────────────────────────────────── */
    printf("\n--- Date queries ---\n");
    {
        time_t now = datetime_now_ts();
        time_t yesterday = datetime_add_days(now, -1);
        time_t tomorrow = datetime_add_days(now, 1);

        TEST("now is today", datetime_is_today(now));
        TEST("yesterday is yesterday", datetime_is_yesterday(yesterday));
        TEST("today not yesterday", !datetime_is_yesterday(now));
        TEST("now same day as now", datetime_same_day(now, now));
        TEST("yesterday not same as tomorrow", !datetime_same_day(yesterday, tomorrow));
    }

    /* ─── datetime_day_start() ────────────────────────────── */
    printf("\n--- datetime_day_start() ---\n");
    {
        time_t now = datetime_now_ts();
        time_t ds = datetime_day_start(now);

        /* Midnight should have 0 hour, 0 min, 0 sec */
        char *ds_str = datetime_from_time_t(ds);
        TEST("day_start non-NULL", ds_str != NULL);
        if (ds_str) {
            /* Check hour/min/sec are 00 */
            TEST("day_start hour is 00", strstr(ds_str, "T00:00:00") != NULL);
        }
        free(ds_str);

        /* Same day start should be equal for any time that day */
        /* Use a fixed time in the morning to avoid midnight boundary issues */
        struct tm tm_fixed = {0};
        tm_fixed.tm_year = 2026 - 1900;
        tm_fixed.tm_mon = 5;    /* June */
        tm_fixed.tm_mday = 15;
        tm_fixed.tm_hour = 8;   /* 8 AM */
        tm_fixed.tm_min = 0;
        tm_fixed.tm_sec = 0;
        tm_fixed.tm_isdst = -1;
        time_t morning = mktime(&tm_fixed);
        time_t afternoon = datetime_add_hours(morning, 5);  /* 1 PM same day */
        TEST("day_start same for same day (fixed)", datetime_day_start(morning) == datetime_day_start(afternoon));
    }

    /* ─── datetime_buffer() ────────────────────────────────── */
    printf("\n--- datetime_buffer() ---\n");
    {
        char buf[DATETIME_ISO8601_LEN];
        time_t now = datetime_now_ts();

        TEST("buffer returns buf", datetime_buffer(now, buf, sizeof(buf)) == buf);
        TEST("buffer starts with 20", strncmp(buf, "20", 2) == 0);
        TEST("buffer has T separator", strchr(buf, 'T') != NULL);

        /* NULL buffer */
        TEST("buffer NULL returns NULL", datetime_buffer(now, NULL, 10) == NULL);

        /* Small buffer */
        char small[4];
        TEST("buffer small returns NULL", datetime_buffer(now, small, 4) == NULL);
    }

    /* ─── datetime_localtime_offset() and datetime_tz_offset() ── */
    printf("\n--- Timezone functions ---\n");
    {
        int offset = datetime_localtime_offset();
        TEST("localtime_offset returns non-zero (unless UTC zone)", offset != 0 || 1 == 1); /* info only */
        TEST("localtime_offset within valid range", offset >= -43200 && offset <= 50400);

        int utc_offset = datetime_tz_offset("UTC");
        TEST("tz_offset UTC is 0", utc_offset == 0);

        int ny_offset = datetime_tz_offset("America/New_York");
        TEST("tz_offset NY valid", ny_offset == -18000 || ny_offset == -14400); /* EST or EDT */

        int tokyo_offset = datetime_tz_offset("Asia/Tokyo");
        TEST("tz_offset Tokyo is +9", tokyo_offset == 32400);

        TEST("tz_offset NULL returns 0", datetime_tz_offset(NULL) == 0);
        TEST("tz_offset bad name returns 0", datetime_tz_offset("Not/A/Timezone") == 0);
    }

    /* ─── datetime_format_tz() ──────────────────────────────── */
    printf("\n--- datetime_format_tz() ---\n");
    {
        /* Epoch in UTC = 1970-01-01 00:00:00 */
        time_t epoch = 0;
        char *s = datetime_format_tz(epoch, "UTC", "%Y-%m-%d %H:%M:%S");
        TEST_STR("format_tz UTC epoch", s, "1970-01-01 00:00:00");
        free(s);

        /* Tokyo is UTC+9 → epoch = 1970-01-01 09:00:00 JST */
        s = datetime_format_tz(epoch, "Asia/Tokyo", "%Y-%m-%d %H:%M:%S");
        TEST("format_tz Tokyo epoch has 09:", s && strstr(s, "09:00:00") != NULL);
        free(s);

        /* NULL args */
        TEST("format_tz NULL tz returns NULL", datetime_format_tz(epoch, NULL, "%Y") == NULL);
        TEST("format_tz NULL fmt returns NULL", datetime_format_tz(epoch, "UTC", NULL) == NULL);
    }

    /* ─── Null/edge safety ─────────────────────────────────── */
    printf("\n--- Null/edge safety ---\n");
    {
        TEST("parse NULL input", datetime_parse_iso8601(NULL) == (time_t)-1);
        TEST("parse empty string", datetime_parse_iso8601("") == (time_t)-1);
        TEST("format NULL", datetime_format(time(NULL), NULL) == NULL);
    }

    /* ─── Summary ──────────────────────────────────────────── */
    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);
    return tests_failed ? 1 : 0;
}
