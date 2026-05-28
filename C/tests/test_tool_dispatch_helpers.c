/* test_tool_dispatch_helpers.c — Tests for libtooldispatch (5 functions). */
#include "tool_dispatch_helpers.h"
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
    printf("=== Tool Dispatch Helpers Tests ===\n\n");

    /* ---- is_destructive_command ---- */
    printf("--- is_destructive_command ---\n");
    TEST("rm -rf / is destructive",
         is_destructive_command("rm -rf /"));
    TEST("rm file.txt is destructive",
         is_destructive_command("rm file.txt"));
    TEST("rmdir folder is destructive",
         is_destructive_command("rmdir folder"));
    TEST("cp -r src dst is destructive",
         is_destructive_command("cp -r src dst"));
    TEST("mv old new is destructive",
         is_destructive_command("mv old new"));
    TEST("install -m 755 file /bin/ is destructive",
         is_destructive_command("install -m 755 file /bin/"));
    TEST("sed -i is destructive",
         is_destructive_command("sed -i 's/foo/bar/' file"));
    TEST("truncate -s 0 file is destructive",
         is_destructive_command("truncate -s 0 file"));
    TEST("dd if=/dev/zero of=file is destructive",
         is_destructive_command("dd if=/dev/zero of=file"));
    TEST("shred -u file is destructive",
         is_destructive_command("shred -u file"));
    TEST("git reset --hard is destructive",
         is_destructive_command("git reset --hard"));
    TEST("git clean -fd is destructive",
         is_destructive_command("git clean -fd"));
    TEST("git checkout -- file is destructive",
         is_destructive_command("git checkout -- file"));
    TEST("output redirect > is destructive",
         is_destructive_command("echo hello > file.txt"));
    TEST("append >> is NOT destructive",
         !is_destructive_command("echo hello >> file.txt"));
    TEST("ls -la is NOT destructive",
         !is_destructive_command("ls -la"));
    TEST("grep pattern file is NOT destructive",
         !is_destructive_command("grep pattern file"));
    TEST("echo hello is NOT destructive",
         !is_destructive_command("echo hello"));
    TEST("cat file is NOT destructive",
         !is_destructive_command("cat file"));
    TEST("empty string is NOT destructive",
         !is_destructive_command(""));
    TEST("NULL is NOT destructive",
         !is_destructive_command(NULL));
    TEST("stream is NOT destructive (starts with cat-like)",
         !is_destructive_command("git diff"));
    TEST("mkdir is NOT destructive",
         !is_destructive_command("mkdir -p newdir"));

    /* ---- extract_error_preview ---- */
    printf("\n--- extract_error_preview ---\n");

    /* JSON error extraction */
    {
        char *e = extract_error_preview("{\"error\":\"file not found\"}", 100);
        TEST("extracts error from JSON", e && strcmp(e, "file not found") == 0);
        free(e);
    }
    /* Extract error preview — JSON with no error field returns raw JSON text */
    {
        char *e = extract_error_preview("{\"result\":\"ok\"}", 100);
        TEST("no error field returns raw JSON text",
             e && strstr(e, "result") != NULL && strstr(e, "ok") != NULL);
        free(e);
    }
    {
        char *e = extract_error_preview("not json at all", 100);
        TEST("non-JSON uses raw text", e && strcmp(e, "not json at all") == 0);
        free(e);
    }
    {
        char *e = extract_error_preview("", 100);
        TEST("empty string returns NULL", e == NULL);
    }
    {
        char *e = extract_error_preview(NULL, 100);
        TEST("NULL input returns NULL", e == NULL);
    }
    {
        char *e = extract_error_preview("Too   many    spaces", 100);
        TEST("collapses whitespace", e && strcmp(e, "Too many spaces") == 0);
        free(e);
    }
    {
        char *e = extract_error_preview("A very long error message that should be truncated", 20);
        TEST("truncates at max_len",
             e && strlen(e) < 25 && strncmp(e, "A very long", 11) == 0);
        free(e);
    }
    {
        char *e = extract_error_preview("{\"error\":\"Permission denied\",\"code\":403}", 100);
        TEST("extracts error from complex JSON",
             e && strcmp(e, "Permission denied") == 0);
        free(e);
    }

    /* ---- extract_file_mutation_targets ---- */
    printf("\n--- extract_file_mutation_targets ---\n");

    {
        size_t cnt;
        char **t = extract_file_mutation_targets("write_file",
            "{\"path\":\"/tmp/test.txt\"}", &cnt);
        TEST("write_file extracts path", t && cnt == 1 &&
             strcmp(t[0], "/tmp/test.txt") == 0);
        free_mutation_targets(t, cnt);
    }
    {
        size_t cnt;
        char **t = extract_file_mutation_targets("patch",
            "{\"path\":\"src/main.c\",\"old_string\":\"foo\",\"new_string\":\"bar\"}", &cnt);
        TEST("patch replace mode extracts path", t && cnt == 1 &&
             strcmp(t[0], "src/main.c") == 0);
        free_mutation_targets(t, cnt);
    }
    {
        size_t cnt;
        char **t = extract_file_mutation_targets("write_file",
            "{\"name\":\"test\"}", &cnt);
        TEST("write_file without path returns empty", t == NULL && cnt == 0);
        free_mutation_targets(t, cnt);
    }
    {
        size_t cnt;
        char **t = extract_file_mutation_targets(NULL, "{}", &cnt);
        TEST("NULL tool_name returns empty", t == NULL && cnt == 0);
        free_mutation_targets(t, cnt);
    }
    {
        size_t cnt;
        char **t = extract_file_mutation_targets("unknown_tool", "{\"path\":\"x\"}", &cnt);
        TEST("unknown tool returns empty", t == NULL && cnt == 0);
        free_mutation_targets(t, cnt);
    }
    {
        size_t cnt;
        char **t = extract_file_mutation_targets("patch",
            "{\"mode\":\"patch\",\"patch\":\"*** Begin Patch\\n*** Update File: src/a.c\\n@@ -1 +1 @@\\n-foo\\n+bar\\n*** Update File: src/b.c\\n@@ -1 +1 @@\\n-x\\n+y\\n*** End Patch\"}", &cnt);
        TEST("patch mode extracts multiple files",
             t && cnt == 2 &&
             strcmp(t[0], "src/a.c") == 0 &&
             strcmp(t[1], "src/b.c") == 0);
        free_mutation_targets(t, cnt);
    }

    /* ---- is_multimodal_tool_result ---- */
    printf("\n--- is_multimodal_tool_result ---\n");

    TEST("multimodal with content array is detected",
         is_multimodal_tool_result("{\"_multimodal\":true,\"content\":[]}"));
    TEST("multimodal with non-array content is NOT detected",
         !is_multimodal_tool_result("{\"_multimodal\":true,\"content\":\"text\"}"));
    TEST("no _multimodal returns false",
         !is_multimodal_tool_result("{\"content\":[]}"));
    TEST("_multimodal false returns false",
         !is_multimodal_tool_result("{\"_multimodal\":false,\"content\":[]}"));
    TEST("empty string returns false",
         !is_multimodal_tool_result(""));
    TEST("NULL returns false",
         !is_multimodal_tool_result(NULL));
    TEST("_multimodal=true without content returns false",
         !is_multimodal_tool_result("{\"_multimodal\":true}"));
    TEST("_multimodal=true with empty content array is multimodal",
         is_multimodal_tool_result("{\"_multimodal\":true,\"content\":[]}"));
    TEST("_multimodal=true with non-empty content array is multimodal",
         is_multimodal_tool_result("{\"_multimodal\":true,\"content\":[{\"type\":\"text\",\"text\":\"hello\"}]}"));

    /* ---- paths_overlap ---- */
    printf("\n--- paths_overlap ---\n");

    TEST("identical paths overlap",
         paths_overlap("/home/user/project", "/home/user/project"));
    TEST("parent and child overlap",
         paths_overlap("/home/user", "/home/user/project"));
    TEST("child and parent overlap",
         paths_overlap("/home/user/project", "/home/user"));
    TEST("sibling paths do NOT overlap",
         !paths_overlap("/home/user/a", "/home/user/b"));
    TEST("unrelated paths do NOT overlap",
         !paths_overlap("/home/user", "/tmp"));
    TEST("empty left returns false",
         !paths_overlap("", "/tmp"));
    TEST("NULL left returns false",
         !paths_overlap(NULL, "/tmp"));
    TEST("NULL right returns false",
         !paths_overlap("/tmp", NULL));
    TEST("NULL both returns false",
         !paths_overlap(NULL, NULL));
    TEST("deeply nested child",
         paths_overlap("/a/b/c", "/a/b/c/d/e/f"));
    TEST("root and subdir",
         paths_overlap("/", "/usr"));

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
