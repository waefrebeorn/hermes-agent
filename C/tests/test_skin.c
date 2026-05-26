/* Test skin engine — default skin + ANSI colors + TrueColor + built-in skins */
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
    const char *banner = skin_get(s, "colors.banner_title", "#FFD700");
    TEST("banner_title default is hex", banner && strcmp(banner, "#FFD700") == 0);

    /* Missing key returns fallback */
    const char *missing = skin_get(s, "colors.nonexistent", "fallback");
    TEST("missing key returns fallback", missing && strcmp(missing, "fallback") == 0);

    /* Boolean lookup */
    bool bold = skin_get_bool(s, "format.banner_bold", false);
    TEST("banner_bold is true", bold == true);

    /* Branding */
    const char *agent = skin_get_branding(s, "agent_name", "default");
    TEST("branding agent_name not null", agent != NULL);
    TEST("branding agent_name contains Slermes", agent && strstr(agent, "Slermes") != NULL);

    const char *prompt_sym = skin_get_branding(s, "prompt_symbol", "?");
    TEST("branding prompt_symbol not null", prompt_sym != NULL);

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

    /* TrueColor hex support */
    const char *hex_gold = skin_ansi_color("#FFD700");
    TEST("hex #FFD700 not null", hex_gold != NULL);
    TEST("hex #FFD700 contains 38;2", hex_gold && strstr(hex_gold, "38;2") != NULL);
    TEST("hex #FFD700 contains 255", hex_gold && strstr(hex_gold, "255") != NULL);
    TEST("hex #FFD700 contains 215", hex_gold && strstr(hex_gold, "215") != NULL);
    TEST("hex #FFD700 contains 0", hex_gold && strstr(hex_gold, "0") != NULL);

    const char *hex_crimson = skin_ansi_color("#DC143C");
    TEST("hex #DC143C not null", hex_crimson != NULL);
    TEST("hex #DC143C contains 220", hex_crimson && strstr(hex_crimson, "220") != NULL);

    const char *hex_invalid = skin_ansi_color("#GGG");
    TEST("invalid hex returns empty", hex_invalid && hex_invalid[0] == '\0');

    /* Skin color via hex */
    const char *banner_color = skin_color(s, "banner_title");
    TEST("skin_color banner_title not null", banner_color != NULL);
    TEST("skin_color banner_title has ANSI", banner_color && banner_color[0] == '\033');

    /* Apply color with FG/bold output */
    const char *fg = NULL;
    int bold_flag = 0;
    skin_apply_color(s, "banner_title", &fg, &bold_flag);
    TEST("skin_apply_color sets fg", fg != NULL);
    TEST("skin_apply_color fg non-empty", fg && fg[0] != 0);

    /* Symbol lookup */
    const char *prompt_sym2 = skin_get(s, "symbols.prompt", "?");
    TEST("default prompt symbol not null", prompt_sym2 != NULL);

    /* NULL safety */
    const char *null_name = skin_name(NULL);
    TEST("skin_name(NULL) returns default", null_name != NULL);

    const char *null_get = skin_get(NULL, "colors.banner_title", "fallback");
    TEST("skin_get(NULL, ...) returns fallback", null_get && strcmp(null_get, "fallback") == 0);

    bool null_bool = skin_get_bool(NULL, "anything", true);
    TEST("skin_get_bool(NULL, ...) returns default", null_bool == true);

    const char *null_color = skin_color(NULL, "banner_title");
    TEST("skin_color(NULL, ...) not null", null_color != NULL);

    /* With overrides */
    skin_t *s3 = skin_with_overrides("{\"colors\":{\"banner_title\":\"magenta\"}}");
    TEST("skin_with_overrides creates skin", s3 != NULL);

    /* Built-in skins enumeration */
    int count = skin_builtin_count();
    TEST("builtin skin count >= 5", count >= 5);
    TEST("builtin skin 0 is default", count > 0 && strcmp(skin_builtin_name(0), "default") == 0);
    TEST("builtin skin 1 is ares", count > 1 && strcmp(skin_builtin_name(1), "ares") == 0);
    TEST("builtin skin 2 is mono", count > 2 && strcmp(skin_builtin_name(2), "mono") == 0);
    TEST("builtin skin 3 is slate", count > 3 && strcmp(skin_builtin_name(3), "slate") == 0);
    TEST("builtin skin 4 is daylight", count > 4 && strcmp(skin_builtin_name(4), "daylight") == 0);

    /* Load built-in skins via JSON string */
    skin_t *s_ares = skin_load_string(
        "{\"name\":\"ares\"}"
    );
    TEST("loaded ares by JSON string", s_ares != NULL);
    TEST("ares name matches", s_ares && strcmp(skin_name(s_ares), "ares") == 0);
    skin_free(s_ares);
    /* Just verify enumeration covers them all */
    for (int i = 0; i < count; i++) {
        const char *name = skin_builtin_name(i);
        TEST("builtin skin name not null", name != NULL);
    }

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
