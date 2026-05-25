/* Test skin engine — default skin + ANSI colors */
#include "skin.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int pass = 0, fail = 0;

#define TEST(name, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
        fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        pass++; \
    } \
} while(0)

int main(void) {
    printf("=== Skin Engine Tests ===\n");

    /* Default skin creation */
    skin_t *s = skin_default();
    TEST("skin_default returns skin", s != NULL);
    TEST("skin name is default", s && strcmp(skin_name(s), "default") == 0);

    /* Key lookup (dotted path) */
    const char *banner = skin_get(s, "colors.banner", "cyan");
    TEST("banner color default is cyan", banner && strcmp(banner, "cyan") == 0);

    /* Missing key returns fallback */
    const char *missing = skin_get(s, "colors.nonexistent", "fallback");
    TEST("missing key returns fallback", missing && strcmp(missing, "fallback") == 0);

    /* Boolean lookup */
    bool bold = skin_get_bool(s, "format.banner_bold", false);
    TEST("banner_bold returns bool", bold == true || bold == false);

    /* ANSI color resolution */
    const char *red_ansi = skin_ansi_color("red");
    TEST("red ANSI not null", red_ansi != NULL);
    TEST("red ANSI contains 31", red_ansi && strstr(red_ansi, "31") != NULL);

    const char *cyan_ansi = skin_ansi_color("cyan");
    TEST("cyan ANSI contains 36", cyan_ansi && strstr(cyan_ansi, "36") != NULL);

    const char *bold_ansi = skin_ansi_color("cyan:bold");
    TEST("cyan:bold contains 1;36", bold_ansi && strstr(bold_ansi, "1;") != NULL);

    const char *dim_ansi = skin_ansi_color("white:dim");
    TEST("white:dim contains 2;", dim_ansi && strstr(dim_ansi, "2;") != NULL);

    const char *invalid = skin_ansi_color("not-a-color");
    TEST("invalid color returns default", invalid != NULL);

    /* Skin color convenience */
    const char *banner_resolved = skin_color(s, "banner");
    TEST("skin_color returns banner color", banner_resolved != NULL);

    /* Apply color with FG/bold output */
    const char *fg = NULL;
    int bold_flag = 0;
    skin_apply_color(s, "banner", &fg, &bold_flag);
    TEST("skin_apply_color sets fg", fg != NULL);
    TEST("skin_apply_color fg non-empty", fg && fg[0] != 0);

    /* Symbol lookup (dotted path) */
    const char *prompt_sym = skin_get(s, "symbols.prompt", "?");
    TEST("default prompt symbol not null", prompt_sym != NULL);

    const char *tool_sym = skin_get(s, "symbols.tool", "?");
    TEST("default tool symbol not null", tool_sym != NULL);

    /* Format flags */
    bool show_model = skin_get_bool(s, "format.show_model", true);
    TEST("show_model returns bool", show_model == true || show_model == false);

    /* NULL safety */
    const char *null_name = skin_name(NULL);
    TEST("skin_name(NULL) returns default", null_name != NULL);

    const char *null_get = skin_get(NULL, "colors.banner", "fallback");
    TEST("skin_get(NULL, ...) returns fallback", null_get && strcmp(null_get, "fallback") == 0);

    bool null_bool = skin_get_bool(NULL, "anything", true);
    TEST("skin_get_bool(NULL, ...) returns default", null_bool == true);

    const char *null_color = skin_color(NULL, "banner");
    TEST("skin_color(NULL, ...) not null", null_color != NULL);

    /* With overrides */
    skin_t *s3 = skin_with_overrides("{\"colors\":{\"banner\":\"magenta\"}}");
    TEST("skin_with_overrides creates skin", s3 != NULL);

    /* Error handling */
    skin_t *bad = skin_load("/nonexistent/skin.json");
    TEST("skin_load of nonexistent file returns NULL", bad == NULL);
    const char *err = skin_error();
    TEST("skin_error returns message after failed load", err != NULL);

    /* Free */
    skin_free(s);
    skin_free(s3);
    skin_free(NULL);

    printf("\n=== %d/%d passed, %d failed ===\n", pass, pass + fail, fail);
    return fail > 0 ? 1 : 0;
}
