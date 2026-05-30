/*
 * test_html.c — Tests for libhtml (HTML escaping/unescaping).
 */
#include "html.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int total = 0, passed = 0;

#define TEST(name, expr) do { \
    total++; \
    if (!(expr)) { fprintf(stderr, "  FAIL: %s (line %d)\n", name, __LINE__); } \
    else { passed++; } \
} while(0)

int main(void) {
    char *got;

    /* Escape */
    got = html_escape(NULL);
    TEST("escape NULL", got == NULL);

    got = html_escape("");
    TEST("escape empty", got && strcmp(got, "") == 0);
    free(got);

    got = html_escape("hello");
    TEST("escape plain", got && strcmp(got, "hello") == 0);
    free(got);

    got = html_escape("<script>");
    TEST("escape < >", got && strcmp(got, "&lt;script&gt;") == 0);
    free(got);

    got = html_escape("\"hello\" & 'world'");
    TEST("escape quotes &", got && strcmp(got, "&quot;hello&quot; &amp; &#39;world&#39;") == 0);
    free(got);

    /* Unescape */
    got = html_unescape("&lt;b&gt;bold&lt;/b&gt;");
    TEST("unescape basic", got && strcmp(got, "<b>bold</b>") == 0);
    free(got);

    got = html_unescape("&amp;amp;");
    TEST("unescape double", got && strcmp(got, "&amp;") == 0);
    free(got);

    got = html_unescape("&#65;&#x42;&#67;");
    TEST("unescape numeric", got && strcmp(got, "ABC") == 0);
    free(got);

    /* Round-trip */
    got = html_escape("a<b>c&d\"e'f");
    TEST("roundtrip encode not null", got != NULL);
    if (got) {
        char *dec = html_unescape(got);
        TEST("roundtrip decode not null", dec != NULL);
        TEST("roundtrip match", dec && strcmp(dec, "a<b>c&d\"e'f") == 0);
        free(dec);
        free(got);
    }

    /* Strip tags */
    got = html_strip_tags("<b>Hello</b> <i>World</i>");
    TEST("strip basic", got && strcmp(got, "Hello World") == 0);
    free(got);

    got = html_strip_tags("<div class=\"test\">text</div>");
    TEST("strip with attrs", got && strcmp(got, "text") == 0);
    free(got);

    got = html_strip_tags("no tags here");
    TEST("strip no tags", got && strcmp(got, "no tags here") == 0);
    free(got);

    got = html_strip_tags("");
    TEST("strip empty", got && strcmp(got, "") == 0);
    free(got);

    /* YAML frontmatter */
    got = strip_yaml_frontmatter(NULL);
    TEST("frontmatter NULL", got == NULL);

    got = strip_yaml_frontmatter("Hello world");
    TEST("frontmatter no ---", got && strcmp(got, "Hello world") == 0);
    free(got);

    got = strip_yaml_frontmatter("---\ntitle: Doc\n---\n# Body\nContent.");
    TEST("frontmatter stripped", got && strcmp(got, "# Body\nContent.") == 0);
    free(got);

    got = strip_yaml_frontmatter("---\nkey: val\n---");
    TEST("frontmatter only returns original", got && strcmp(got, "---\nkey: val\n---") == 0);
    free(got);

    got = strip_yaml_frontmatter("---\nunclosed");
    TEST("frontmatter unclosed returns original", got && strcmp(got, "---\nunclosed") == 0);
    free(got);

    got = strip_yaml_frontmatter("---\nkey: val\n---\n\n\nBody after blanks.");
    TEST("frontmatter trailing newlines stripped", got && strcmp(got, "Body after blanks.") == 0);
    free(got);

    fprintf(stderr, "html: %d/%d passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
