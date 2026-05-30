/*
 * skill_commands.c — Skill slash-command support for Hermes C.
 * Port of Python agent/skill_commands.py (523 lines).
 *
 * Provides:
 * - skill_cmd_scan() — scan ~/.hermes/skills/, parse frontmatter, build /slug cache
 * - skill_cmd_get() / skill_cmd_get_all() — cached accessors
 * - skill_cmd_resolve() — normalize user input to canonical slug
 * - skill_cmd_build_message() — load SKILL.md, build formatted invocation message
 * - skill_cmd_rescan() — re-scan and return diff
 */

#include "hermes_skill_commands.h"
#include "hermes_system_prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

/* ================================================================
 *  Internal state
 * ================================================================ */

static skill_cmd_entry_t g_skills[MAX_SKILL_COMMANDS];
static int g_skill_count = 0;
static time_t g_last_scan = 0;
static char g_skills_dir[SKILL_CMD_PATH_MAX];

/* ================================================================
 *  Helpers
 * ================================================================ */

/* Trim trailing whitespace/newlines */
static void trim_right(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t'
           || s[len-1] == '\n' || s[len-1] == '\r'))
        s[--len] = '\0';
}

/* Normalize a skill name to /slug:
 *   lowercase, spaces_underscores->hyphens,
 *   strip non-alnum (except hyphens), collapse multi-hyphens */
static void name_to_slug(const char *name, char *slug, size_t slug_sz) {
    if (!name || !*name) { slug[0] = '/'; slug[1] = '\0'; return; }

    char buf[SKILL_CMD_SLUG_MAX];
    size_t pos = 0;
    for (const char *p = name; *p && pos < sizeof(buf) - 1; p++) {
        char c = (char)tolower((unsigned char)*p);
        if (c == ' ' || c == '_') c = '-';
        if (c == '-' || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            buf[pos++] = c;
    }
    buf[pos] = '\0';

    /* Collapse multi-hyphens */
    char out[SKILL_CMD_SLUG_MAX];
    size_t opos = 0;
    int last_hyphen = 0;
    for (size_t i = 0; buf[i] && opos < sizeof(out) - 1; i++) {
        if (buf[i] == '-') {
            if (last_hyphen) continue;
            last_hyphen = 1;
        } else {
            last_hyphen = 0;
        }
        out[opos++] = buf[i];
    }
    out[opos] = '\0';
    /* Strip leading/trailing hyphens */
    while (opos > 0 && out[opos-1] == '-') out[--opos] = '\0';
    const char *start = out;
    while (*start == '-') start++;
    snprintf(slug, slug_sz, "/%s", start);
}

/* Extract "key: value" from a YAML frontmatter line */
static int extract_fm(const char *line, const char *key,
                       char *out, size_t out_sz) {
    size_t klen = strlen(key);
    while (*line == ' ' || *line == '\t') line++;
    if (strncmp(line, key, klen) != 0) return 0;
    line += klen;
    while (*line == ':' || *line == ' ' || *line == '\t') line++;
    if (*line == '>') { snprintf(out, out_sz, "(multi-line)"); return 1; }
    snprintf(out, out_sz, "%s", line);
    trim_right(out);
    return 1;
}

/* Determine skills directory: try HERMES_HOME, HOME, then fallback */
static const char *get_skills_dir(void) {
    static char buf[SKILL_CMD_PATH_MAX];
    if (buf[0]) return buf;

    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";

    snprintf(buf, sizeof(buf), "%s/.hermes/skills", home);
    struct stat st;
    if (stat(buf, &st) != 0 || !S_ISDIR(st.st_mode)) {
        snprintf(buf, sizeof(buf), "%s/.slermes/skills", home);
        if (stat(buf, &st) != 0 || !S_ISDIR(st.st_mode))
            buf[0] = '\0';
    }
    return buf;
}

/* ================================================================
 *  Scanning
 * ================================================================ */

static int scan_skills_dir(void) {
    g_skill_count = 0;
    g_last_scan = time(NULL);

    const char *sdir = get_skills_dir();
    if (!sdir || !sdir[0]) return 0;
    snprintf(g_skills_dir, sizeof(g_skills_dir), "%s", sdir);

    DIR *d = opendir(sdir);
    if (!d) return 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && g_skill_count < MAX_SKILL_COMMANDS) {
        if (entry->d_name[0] == '.') continue;

        char dir_path[SKILL_CMD_PATH_MAX];
        snprintf(dir_path, sizeof(dir_path), "%s/%s", sdir, entry->d_name);

        struct stat st;
        if (stat(dir_path, &st) != 0 || !S_ISDIR(st.st_mode))
            continue;

        /* Must contain SKILL.md */
        char md_path[SKILL_CMD_PATH_MAX + 32];
        snprintf(md_path, sizeof(md_path), "%s/SKILL.md", dir_path);
        if (stat(md_path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        /* Read SKILL.md and parse frontmatter */
        FILE *f = fopen(md_path, "r");
        if (!f) continue;

        skill_cmd_entry_t *sk = &g_skills[g_skill_count];
        memset(sk, 0, sizeof(*sk));
        snprintf(sk->name, sizeof(sk->name), "%s", entry->d_name);

        char line[4096];
        int in_fm = 0, fm_ended = 0;
        int found_name = 0, found_desc = 0;

        while (fgets(line, sizeof(line), f) && !fm_ended) {
            trim_right(line);
            if (strcmp(line, "---") == 0) {
                if (!in_fm) { in_fm = 1; continue; }
                else { fm_ended = 1; continue; }
            }
            if (in_fm) {
                if (!found_name && extract_fm(line, "name", sk->name, sizeof(sk->name)))
                    found_name = 1;
                if (!found_desc && extract_fm(line, "description", sk->description, sizeof(sk->description)))
                    found_desc = 1;
            }
        }
        fclose(f);

        name_to_slug(sk->name, sk->slug, sizeof(sk->slug));
        snprintf(sk->skill_path, sizeof(sk->skill_path), "%s", dir_path);
        g_skill_count++;
    }
    closedir(d);
    return g_skill_count;
}

/* ================================================================
 *  Public API
 * ================================================================ */

int skill_cmd_scan(void) {
    return scan_skills_dir();
}

const skill_cmd_entry_t *skill_cmd_get(const char *slug) {
    if (!slug || !g_skill_count) return NULL;
    for (int i = 0; i < g_skill_count; i++) {
        if (strcmp(g_skills[i].slug, slug) == 0)
            return &g_skills[i];
    }
    return NULL;
}

const skill_cmd_entry_t *skill_cmd_get_all(int *count) {
    if (count) *count = g_skill_count;
    return g_skills;
}

const char *skill_cmd_resolve(const char *command) {
    if (!command || !g_skill_count) return NULL;

    /* Strip leading slashes */
    const char *cmd = command;
    while (*cmd == '/') cmd++;
    if (!*cmd) return NULL;

    /* Normalize: underscore->hyphen, lowercase */
    char normalized[SKILL_CMD_SLUG_MAX];
    size_t pos = 0;
    for (const char *p = cmd; *p && pos < sizeof(normalized) - 1; p++) {
        char c = (char)tolower((unsigned char)*p);
        if (c == '_') c = '-';
        normalized[pos++] = c;
    }
    normalized[pos] = '\0';

    /* Build full /slug and search */
    char full_slug[SKILL_CMD_SLUG_MAX];
    snprintf(full_slug, sizeof(full_slug), "/%s", normalized);

    for (int i = 0; i < g_skill_count; i++) {
        if (strcmp(g_skills[i].slug, full_slug) == 0)
            return g_skills[i].slug;
    }
    return NULL;
}

char *skill_cmd_build_message(const char *slug, const char *user_args) {
    if (!slug) return NULL;

    const skill_cmd_entry_t *sk = skill_cmd_get(slug);
    if (!sk) return NULL;

    /* Read SKILL.md */
    char md_path[SKILL_CMD_PATH_MAX + 32];
    snprintf(md_path, sizeof(md_path), "%s/SKILL.md", sk->skill_path);

    FILE *f = fopen(md_path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz > 100000) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);

    char *content = (char *)malloc((size_t)sz + 1);
    if (!content) { fclose(f); return NULL; }
    size_t n = fread(content, 1, (size_t)sz, f);
    fclose(f);
    content[n] = '\0';

    /* Strip frontmatter */
    char *body = context_strip_frontmatter(content);

    /* Build invocation message */
    size_t total = 4096 + strlen(body) + 4096;
    char *msg = (char *)malloc(total);
    if (!msg) { free(body); free(content); return NULL; }

    int written = snprintf(msg, total,
        "[IMPORTANT: The user has invoked the \"%s\" skill, indicating they want "
        "you to follow its instructions. The full skill content is loaded below.]\n"
        "\n"
        "%s\n"
        "\n"
        "[Skill directory: %s]\n"
        "Resolve any relative paths in this skill (e.g. `scripts/foo.js`, "
        "`templates/config.yaml`) against that directory, then run them "
        "with the terminal tool using the absolute path.\n",
        sk->name, body, sk->skill_path);

    if (written < 0) { free(msg); free(body); free(content); return NULL; }

    /* List supporting files from subdirectories */
    const char *subdirs[] = {"references", "templates", "scripts", "assets"};
    char supporting[16384] = "";
    size_t sp = 0;

    for (int i = 0; i < 4; i++) {
        char sub_path[SKILL_CMD_PATH_MAX + 32];
        snprintf(sub_path, sizeof(sub_path), "%s/%s", sk->skill_path, subdirs[i]);
        struct stat st;
        if (stat(sub_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            DIR *sd = opendir(sub_path);
            if (sd) {
                struct dirent *e;
                while ((e = readdir(sd)) != NULL) {
                    if (e->d_name[0] == '.') continue;
                    char full[SKILL_CMD_PATH_MAX + 64];
                    snprintf(full, sizeof(full), "%s/%s", sub_path, e->d_name);
                    struct stat fst;
                    if (stat(full, &fst) == 0 && S_ISREG(fst.st_mode)) {
                        size_t remain = sizeof(supporting) - sp;
                        int added = snprintf(supporting + sp, remain,
                            "- %s/%s  ->  %s\n", subdirs[i], e->d_name, full);
                        if (added > 0 && (size_t)added < remain)
                            sp += (size_t)added;
                    }
                }
                closedir(sd);
            }
        }
    }

    if (sp > 0) {
        size_t cur = strlen(msg);
        size_t remain = total - cur;
        snprintf(msg + cur, remain,
            "\n[This skill has supporting files:]\n"
            "%s\n"
            "Load any of these with skill_view(name=\"%s\", "
            "file_path=\"<path>\"), or run scripts directly by absolute path "
            "(e.g. `node %s/scripts/foo.js`).\n",
            supporting, sk->slug + 1, sk->skill_path);
    }

    /* Append user instruction */
    if (user_args && user_args[0]) {
        size_t cur = strlen(msg);
        size_t remain = total - cur;
        snprintf(msg + cur, remain,
            "\nThe user has provided the following instruction alongside the skill invocation: %s\n",
            user_args);
    }

    free(body);
    free(content);
    return msg;
}

int skill_cmd_rescan(int *added, int *removed) {
    /* Snapshot: save old slugs */
    char old_slugs[MAX_SKILL_COMMANDS][SKILL_CMD_SLUG_MAX];
    int old_count = g_skill_count;
    for (int i = 0; i < old_count; i++)
        snprintf(old_slugs[i], sizeof(old_slugs[i]), "%s", g_skills[i].slug);

    /* Re-scan */
    int new_count = scan_skills_dir();

    /* Count added: in new but not in old */
    int n_added = 0, n_removed = 0;
    for (int i = 0; i < new_count; i++) {
        int found = 0;
        for (int j = 0; j < old_count; j++) {
            if (strcmp(g_skills[i].slug, old_slugs[j]) == 0) { found = 1; break; }
        }
        if (!found) n_added++;
    }

    /* Count removed: in old but not in new */
    for (int j = 0; j < old_count; j++) {
        int found = 0;
        for (int i = 0; i < new_count; i++) {
            if (strcmp(g_skills[i].slug, old_slugs[j]) == 0) { found = 1; break; }
        }
        if (!found) n_removed++;
    }

    if (added) *added = n_added;
    if (removed) *removed = n_removed;
    return n_added + n_removed;
}
