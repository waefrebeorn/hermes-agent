/*
 * test_url_safety.c — URL safety module tests.
 * Tests SSRF protection, IP blocking, blocklist, secret detection.
 *
 * Build:
 *   gcc -O2 -g -Wall -I include tests/test_url_safety.c src/tools/url_safety.c \
 *       lib/libjson/json.c lib/libhttp/http.c -o /tmp/hermes_test_url_safety \
 *       -lssl -lcrypto -lz -lm -Wl,--unresolved-symbols=ignore-all
 */
#include "hermes.h"
#include "hermes_url_safety.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

int main(void) {
    printf("=== URL Safety Tests ===\n");

    /* 1. extract_hostname */
    TEST("extract hostname from https URL",
         strcmp(url_extract_hostname("https://example.com/path"), "example.com") == 0);
    TEST("extract hostname from http URL with port",
         strcmp(url_extract_hostname("http://hostname:8080/path"), "hostname") == 0);
    TEST("extract hostname from URL without path",
         strcmp(url_extract_hostname("https://google.com"), "google.com") == 0);

    /* 2. url_host_matches */
    TEST("host matches exact",
         url_host_matches("https://example.com/path", "example.com"));
    TEST("host matches subdomain",
         url_host_matches("https://sub.example.com/path", "example.com"));
    TEST("host does not match different",
         !url_host_matches("https://other.com/path", "example.com"));

    /* 3. url_has_valid_scheme */
    TEST("valid https scheme",
         url_has_valid_scheme("https://example.com"));
    TEST("valid http scheme",
         url_has_valid_scheme("http://example.com"));
    TEST("data scheme not valid for network safety",
         !url_has_valid_scheme("data:image/png;base64,iVBOR"));
    TEST("invalid scheme",
         !url_has_valid_scheme("ftp://example.com"));
    TEST("no scheme (path only)",
         !url_has_valid_scheme("/local/path/file.txt"));

    /* 4. url_has_secret */
    TEST("detect sk- secret in URL",
         url_has_secret("https://example.com?key=sk-proj-abc123") != NULL);
    TEST("detect ghp_ secret in URL",
         url_has_secret("https://example.com?token=ghp_abc123def456") != NULL);
    TEST("detect AIza secret in URL",
         url_has_secret("https://example.com?api=AIzaSyABC123DEF456") != NULL);
    TEST("no secret in clean URL",
         url_has_secret("https://example.com/page.html") == NULL);
    TEST("no secret in URL with safe query",
         url_has_secret("https://example.com/search?q=hello") == NULL);

    /* 5. url_is_always_blocked */
    TEST("localhost always blocked",
         url_is_always_blocked("http://localhost:8080"));
    TEST("127.0.0.1 always blocked",
         url_is_always_blocked("http://127.0.0.1/api"));
    TEST("169.254 link-local blocked",
         url_is_always_blocked("http://169.254.1.1"));
    TEST("10.x private blocked",
         url_is_always_blocked("http://10.0.0.1"));
    TEST("172.16 private blocked",
         url_is_always_blocked("http://172.16.0.1"));
    TEST("192.168 private blocked",
         url_is_always_blocked("http://192.168.1.1"));
    TEST("public IP not blocked",
         !url_is_always_blocked("http://93.184.216.34"));
    TEST("public domain not blocked",
         !url_is_always_blocked("https://example.com"));

    /* 6. Blocklist */
    url_blocklist_add_domain("malware.com");
    url_blocklist_add_domain("phishing.test");
    TEST("blocklisted domain detected",
         url_is_always_blocked("https://malware.com/evil"));
    TEST("blocklisted subdomain detected",
         url_is_always_blocked("https://sub.phishing.test/page"));
    TEST("non-blocklisted domain not blocked",
         !url_is_always_blocked("https://safe.com"));
    url_blocklist_remove_domain("malware.com");
    TEST("removed from blocklist no longer blocked",
         !url_is_always_blocked("https://malware.com"));
    url_blocklist_clear();
    TEST("cleared blocklist allows previously added",
         !url_is_always_blocked("https://phishing.test"));

    /* 7. Allow private */
    url_reset_allow_private();

    /* 8. url_safe_for_log */
    {
        char *s;
        s = url_safe_for_log(NULL, 80);
        TEST("safe_for_log NULL returns NULL", s == NULL);
        free(s);
        s = url_safe_for_log("", 80);
        TEST("safe_for_log empty returns NULL", s == NULL);
        s = url_safe_for_log("https://example.com/path/to/file.json", 80);
        TEST("safe_for_log normal URL path", s && strstr(s, "example.com/.../file.json"));
        free(s);
        s = url_safe_for_log("https://user:pass@api.example.com/data", 80);
        TEST("safe_for_log strips userinfo", s && strstr(s, "api.example.com/.../data") && !strstr(s, "user:pass"));
        free(s);
        s = url_safe_for_log("https://example.com", 80);
        TEST("safe_for_log bare domain", s && strcmp(s, "https://example.com") == 0);
        free(s);
        s = url_safe_for_log("just a plain string without protocol", 80);
        TEST("safe_for_log non-URL string truncated", s != NULL);
        free(s);
        s = url_safe_for_log("https://host:8443/path", 80);
        TEST("safe_for_log preserves port", s && strstr(s, "host:8443"));
        free(s);
        s = url_safe_for_log("https://api.openai.com/v1/chat/completions", 30);
        TEST("safe_for_log truncation", s && strlen(s) <= 30);
        free(s);
        s = url_safe_for_log("https://api.example.com/", 80);
        TEST("safe_for_log trailing slash", s && strstr(s, "example.com"));
        free(s);
    }

    /* 8.5. url_extract_basename */
    TEST("url_basename standard URL",
         strcmp(url_extract_basename("https://example.com/path/to/file.json"), "file.json") == 0);
    TEST("url_basename no path",
         strcmp(url_extract_basename("https://example.com"), "") == 0);
    TEST("url_basename query stripped",
         strcmp(url_extract_basename("https://example.com/path/file.txt?key=val"), "file.txt") == 0);
    TEST("url_basename fragment stripped",
         strcmp(url_extract_basename("https://example.com/path/doc.html#section"), "doc.html") == 0);
    TEST("url_basename just filename",
         strcmp(url_extract_basename("https://example.com/file.png"), "file.png") == 0);
    TEST("url_basename NULL returns empty",
         strcmp(url_extract_basename(NULL), "") == 0);
    TEST("url_basename empty returns empty",
         strcmp(url_extract_basename(""), "") == 0);

    /* 8.6. MIME type utilities */
    TEST("guess_mime_type .jpg", strcmp(url_guess_mime_type("photo.jpg"), "image/jpeg") == 0);
    TEST("guess_mime_type .png", strcmp(url_guess_mime_type("img.png"), "image/png") == 0);
    TEST("guess_mime_type .pdf", strcmp(url_guess_mime_type("doc.pdf"), "application/pdf") == 0);
    TEST("guess_mime_type .mp3", strcmp(url_guess_mime_type("song.mp3"), "audio/mpeg") == 0);
    TEST("guess_mime_type .mp4", strcmp(url_guess_mime_type("video.mp4"), "video/mp4") == 0);
    TEST("guess_mime_type .ogg", strcmp(url_guess_mime_type("voice.ogg"), "audio/ogg") == 0);
    TEST("guess_mime_type unknown ext", strcmp(url_guess_mime_type("file.xyz"), "application/octet-stream") == 0);
    TEST("guess_mime_type no ext", strcmp(url_guess_mime_type("README"), "application/octet-stream") == 0);
    TEST("guess_mime_type NULL", strcmp(url_guess_mime_type(NULL), "application/octet-stream") == 0);
    TEST("guess_mime_type empty", strcmp(url_guess_mime_type(""), "application/octet-stream") == 0);
    TEST("is_image_ext .jpg", url_is_image_extension("photo.jpg"));
    TEST("is_image_ext .png", url_is_image_extension("img.png"));
    TEST("is_image_ext .gif", url_is_image_extension("anim.gif"));
    TEST("is_image_ext .webp", url_is_image_extension("pic.webp"));
    TEST("is_image_ext .pdf is NOT image", !url_is_image_extension("doc.pdf"));
    TEST("is_image_ext no ext is NOT image", !url_is_image_extension("README"));
    TEST("is_image_ext NULL is NOT image", !url_is_image_extension(NULL));

    /* 9. url_is_network_accessible */
    TEST("network_access localhost is not accessible",
         !url_is_network_accessible("127.0.0.1"));
    TEST("network_access 127.1 not accessible",
         !url_is_network_accessible("127.0.0.2"));
    TEST("network_access ::1 not accessible",
         !url_is_network_accessible("::1"));
    TEST("network_access public IP accessible",
         url_is_network_accessible("93.184.216.34"));
    TEST("network_access NULL returns true (fail closed)",
         url_is_network_accessible(NULL));
    TEST("network_access empty returns true (fail closed)",
         url_is_network_accessible(""));

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
