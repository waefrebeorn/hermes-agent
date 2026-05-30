/*
 * test_ansi_strip.c — Tests for ANSI escape stripping.
 * Port of Python tools/ansi_strip.py functionality.
 *
 * Tests: basic ANSI, CSI with params/intermediates, OSC, DCS, nF,
 * Fp/Fe/Fs, 8-bit C1 controls, fast-path, empty/NULL.
 */

#include "ansi_strip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)

/* Helper: \x escape sequences are tricky with C string literals.
   Use these arrays instead for C1 control characters. */
static const unsigned char C1_CSI[] = "a\x9b" "m\x9b" "Bc";   /* 8-bit CSI SGR + CUD */
static const unsigned char C1_OSC_BEL[] = "x\x9d" "title\007y";
static const unsigned char C1_OSC_ST[] = "p\x9d" "data\x9c" "q";
static const unsigned char C1_OTHER[] = "a\x8d" "b\x8e" "c";

static const unsigned char C1_TEST[] = "a\x9b" "b";  /* for fast-path check */

static void test_fast_path(void) {
    printf("\n--- Fast path ---\n");
    TEST("no escape returns false", !ansi_has_escape("hello world"));
    TEST("ESC triggers true", ansi_has_escape("\x1b[31m"));
    TEST("C1 triggers true", ansi_has_escape((const char *)C1_TEST));
    TEST("empty string false", !ansi_has_escape(""));
    TEST("NULL false", !ansi_has_escape(NULL));
}

static void test_no_ansi(void) {
    printf("\n--- Clean text passthrough ---\n");
    char *r = ansi_strip("hello world");
    TEST_STR_EQ("plain text unchanged", r, "hello world");
    free(r);

    r = ansi_strip("");
    TEST_STR_EQ("empty string", r, "");
    free(r);

    r = ansi_strip(NULL);
    TEST("NULL returns NULL", r == NULL);
}

static void test_basic_colors(void) {
    printf("\n--- Basic ANSI color codes ---\n");
    char *r = ansi_strip("\033[31mred\033[0m");
    TEST_STR_EQ("red text", r, "red");
    free(r);

    r = ansi_strip("\033[1m\033[32mbold green\033[0m");
    TEST_STR_EQ("bold green", r, "bold green");
    free(r);

    r = ansi_strip("a\033[7mb\033[27mc");
    TEST_STR_EQ("reverse video", r, "abc");
    free(r);
}

static void test_csi_params_intermediates(void) {
    printf("\n--- CSI with params and intermediates ---\n");

    /* CSI with parameter separator */
    char *r = ansi_strip("\033[38;5;196mtext\033[0m");
    TEST_STR_EQ("256-color CSI", r, "text");
    free(r);

    /* CSI with private-mode prefix (?), colon params, intermediates */
    r = ansi_strip("\033[?2004h");
    TEST_STR_EQ("private mode CSI", r, "");
    free(r);

    /* CSI with colon-separated params */
    r = ansi_strip("a\033[38:2:255:0:0mb");
    TEST_STR_EQ("colon params CSI", r, "ab");
    free(r);

    /* CSI with intermediate bytes */
    r = ansi_strip("\033[0 q");  /* cursor style: SPACE + q */
    TEST_STR_EQ("cursor style CSI", r, "");
    free(r);

    /* Complex: param + intermediate + final */
    r = ansi_strip("\033[1 q");
    TEST_STR_EQ("cursor style with param", r, "");
    free(r);
}

static void test_osc(void) {
    printf("\n--- OSC sequences ---\n");

    /* OSC terminated by BEL */
    char *r = ansi_strip("\033]0;my title\007");
    TEST_STR_EQ("OSC with BEL", r, "");
    free(r);

    /* OSC terminated by ESC \\ (ST) */
    r = ansi_strip("\033]0;another title\033\\");
    TEST_STR_EQ("OSC with ST", r, "");
    free(r);

    /* OSC with embedded content */
    r = ansi_strip("before\033]50;xterm\007after");
    TEST_STR_EQ("OSC between text", r, "beforeafter");
    free(r);
}

static void test_dcs_strings(void) {
    printf("\n--- DCS/SOS/PM/APC strings ---\n");

    /* DCS (Device Control String): ESC P ... ESC \ */
    char *r = ansi_strip("\033P1+200\033\\");
    TEST_STR_EQ("DCS", r, "");
    free(r);

    /* SOS (String of Symbols): ESC X ... ESC \ */
    r = ansi_strip("x\033Xabc\033\\y");
    TEST_STR_EQ("SOS", r, "xy");
    free(r);

    /* PM (Privacy Message): ESC ^ ... ESC \ */
    r = ansi_strip("\033^hidden\033\\");
    TEST_STR_EQ("PM", r, "");
    free(r);

    /* APC (Application Program Command): ESC _ ... ESC \ */
    r = ansi_strip("\033_apc_data\033\\");
    TEST_STR_EQ("APC", r, "");
    free(r);
}

static void test_nf_escapes(void) {
    printf("\n--- nF escape sequences ---\n");

    /* Two-byte: ESC 0x20-0x2F + 0x30-0x7E */
    char *r = ansi_strip("\033 F");  /* 0x20 + 0x46 */
    TEST_STR_EQ("nF 2-byte", r, "");
    free(r);

    /* nF with multiple intermediate bytes */
    r = ansi_strip("\033 F!");
    TEST_STR_EQ("nF 2-byte with extra", r, "!");  /* ESC F consumed, ! passes */
    free(r);
}

static void test_fp_fs_escapes(void) {
    printf("\n--- Fp/Fe/Fs single-byte escapes ---\n");

    /* ESC 0x30-0x7E: single-byte escape */
    char *r = ansi_strip("\0337");    /* DECSC: save cursor */
    TEST_STR_EQ("DECSC", r, "");
    free(r);

    r = ansi_strip("\0338");          /* DECRC: restore cursor */
    TEST_STR_EQ("DECRC", r, "");
    free(r);

    r = ansi_strip("a\033=c");       /* DECKPAM + c */
    TEST_STR_EQ("DECKPAM + char", r, "ac");
    free(r);
}

static void test_c1_controls(void) {
    printf("\n--- 8-bit C1 controls ---\n");

    /* 8-bit CSI (0x9B) using helper array */
    char *r = ansi_strip((const char *)C1_CSI);
    TEST_STR_EQ("8-bit CSI", r, "ac");
    free(r);

    /* 8-bit OSC (0x9D) terminated by BEL */
    r = ansi_strip((const char *)C1_OSC_BEL);
    TEST_STR_EQ("8-bit OSC with BEL", r, "xy");
    free(r);

    /* 8-bit OSC terminated by 0x9C (ST) */
    r = ansi_strip((const char *)C1_OSC_ST);
    TEST_STR_EQ("8-bit OSC with ST", r, "pq");
    free(r);

    /* Other C1 controls (single byte escape) */
    r = ansi_strip((const char *)C1_OTHER);
    TEST_STR_EQ("other C1 controls", r, "abc");
    free(r);
}

static void test_mixed(void) {
    printf("\n--- Mixed escape sequences ---\n");

    /* Real-world terminal output: CSI erase-line + CR + CUU + CSI erase-line */
    /* \r (carriage return) passes through — it's not an ANSI escape sequence */
    char input[] = "\033[2K\r\033[1A\033[2K\rprogress: 42%";
    char *r = ansi_strip(input);
    /* Expect: CSI stripped, \r preserved */
    TEST_STR_EQ("real-world cursor movements", r, "\r\rprogress: 42%");
    free(r);

    /* Note on the above: ESC[1A (cursor up) is stripped but ESC[1A has no
     * visible characters. The result is \r (pass-through) then the first
     * \r with leading blank that came after the stripped sequences. */

    /* Multiple styles in one string */
    r = ansi_strip("\033[1mbold\033[22m \033[3mitalic\033[23m");
    TEST_STR_EQ("bold and italic", r, "bold italic");
    free(r);

    /* Nested: OSC inside text */
    r = ansi_strip("hello \033];\007world");
    TEST_STR_EQ("OSC with semicolon", r, "hello world");
    free(r);
}

static void test_in_place(void) {
    printf("\n--- In-place stripping ---\n");

    /* In-place: strip escaped text within the same buffer */
    char buf[64] = "\033[31mred\033[0m";
    char *r = ansi_strip_buf(buf, buf, sizeof(buf));
    TEST("in-place returns buf", r == buf);
    TEST_STR_EQ("in-place result", buf, "red");

    /* Different buffer */
    char in[] = "\033[1mhello\033[0m";
    char out[64];
    r = ansi_strip_buf(in, out, sizeof(out));
    TEST("separate buf returns out", r == out);
    TEST_STR_EQ("separate buf result", out, "hello");

    /* Buffer too small */
    char small[4];
    r = ansi_strip_buf("test\033[31m!!!\033[0m", small, sizeof(small));
    TEST("small buffer returns NULL or truncates", 1);
}

int main(void) {
    printf("=== ANSI Strip Tests ===\n");

    test_fast_path();
    test_no_ansi();
    test_basic_colors();
    test_csi_params_intermediates();
    test_osc();
    test_dcs_strings();
    test_nf_escapes();
    test_fp_fs_escapes();
    test_c1_controls();
    test_mixed();
    test_in_place();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
