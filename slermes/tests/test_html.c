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

    /* ── Extended edge cases ── */
    printf("\n--- Extended edge cases ---\n");

    /* Escape: all special chars at once */
    got = html_escape("<>&\"'");
    TEST("escape all special", got && strcmp(got, "&lt;&gt;&amp;&quot;&#39;") == 0);
    free(got);

    /* Escape: already-escaped input */
    got = html_escape("&lt;hello&gt;");
    TEST("escape already-escaped", got && strcmp(got, "&amp;lt;hello&amp;gt;") == 0);
    free(got);

    /* Escape: only special chars */
    got = html_escape("<<<>>>&&&");
    TEST("escape only specials", got && strcmp(got, "&lt;&lt;&lt;&gt;&gt;&gt;&amp;&amp;&amp;") == 0);
    free(got);

    /* Escape: long string */
    {
        char long_in[2048];
        memset(long_in, 'a', 2000);
        long_in[2000] = '\0';
        long_in[500] = '<';
        long_in[1000] = '>';
        long_in[1500] = '&';
        got = html_escape(long_in);
        TEST("escape long string non-NULL", got != NULL);
        if (got) {
            TEST("escape long string > 2000", strlen(got) > 2000);
            TEST("escape long contains &lt;", strstr(got, "&lt;") != NULL);
            TEST("escape long contains &gt;", strstr(got, "&gt;") != NULL);
            TEST("escape long contains &amp;", strstr(got, "&amp;") != NULL);
            free(got);
        }
    }

    /* Unescape: NULL input */
    got = html_unescape(NULL);
    TEST("unescape NULL", got == NULL);

    /* Unescape: unknown entity */
    got = html_unescape("&unknown;text");
    TEST("unescape unknown entity", got && strstr(got, "&unknown;") != NULL);
    free(got);

    /* Unescape: invalid numeric entity */
    got = html_unescape("&#XYZ;");
    TEST("unescape invalid numeric", got != NULL);
    free(got);

    /* Unescape: multiple consecutive entities */
    got = html_unescape("&lt;&gt;&amp;&quot;&#39;");
    TEST("unescape multiple entities", got && strcmp(got, "<>&\"'") == 0);
    free(got);

    /* Strip tags: NULL input */
    got = html_strip_tags(NULL);
    TEST("strip NULL returns NULL", got == NULL);

    /* Strip tags: self-closing */
    got = html_strip_tags("line1<br/>line2<br>line3");
    TEST("strip self-closing br", got && strcmp(got, "line1line2line3") == 0);
    free(got);

    /* Strip tags: nested tags */
    got = html_strip_tags("<div><p><b>deep</b></p></div>");
    TEST("strip nested", got && strcmp(got, "deep") == 0);
    free(got);

    /* Strip tags: malformed */
    got = html_strip_tags("<<<>>>");
    TEST("strip malformed <<<>>>", got && strcmp(got, "") == 0);
    free(got);

    /* Frontmatter: just --- with no content */
    got = strip_yaml_frontmatter("---");
    TEST("frontmatter just ---", got && strcmp(got, "---") == 0);
    free(got);

    /* Frontmatter: empty body after --- --- */
    got = strip_yaml_frontmatter("---\n---\n");
    TEST("frontmatter empty body returns original (no body after ---)", got && strcmp(got, "---\n---\n") == 0);
    free(got);

    /* Frontmatter: body after closing --- with leading newline */
    got = strip_yaml_frontmatter("---\nkey: val\n---\nbody");
    TEST("frontmatter with body after ---", got && strcmp(got, "body") == 0);
    free(got);

    /* Frontmatter: body with --- inside (not a delimiter) */
    got = strip_yaml_frontmatter("# Body\n--- not a frontmatter\n--not either");
    TEST("frontmatter --- in body preserved", got && strstr(got, "---") != NULL);
    free(got);

    fprintf(stderr, "html: %d/%d passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
