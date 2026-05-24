/*
 * skill_bundles.c -- Skill bundle loader for C Hermes.
 * Scans ~/.slermes/skill-bundles/YAML files and provides lookup by slug.
 *
 * MIT License — WuBu Hermes Project
 */

#include "skill_bundles.h"
#include "yaml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

/* -----------------------------------------------------------------
 *  Helpers
 * ================================================================ */

/* Slugify a name: lowercase, hyphens, strip invalid chars. */
static void slugify(const char *name, char *out, size_t out_size) {
    if (!name || !out || out_size < 1) return;
    size_t pos = 0;
    int last_hyphen = 0;
    for (size_t i = 0; name[i] && pos < out_size - 1; i++) {
        char c = name[i];
        if (c == ' ' || c == '_') c = '-';
        if (isalnum((unsigned char)c) || c == '-') {
            c = (char)tolower((unsigned char)c);
            if (c == '-' && last_hyphen) continue;
            out[pos++] = c;
            last_hyphen = (c == '-');
        }
    }
    /* Strip leading/trailing hyphens */
    while (pos > 0 && out[pos - 1] == '-') pos--;
    size_t start = 0;
    while (start < pos && out[start] == '-') start++;
    if (start > 0 && start < pos) {
        memmove(out, out + start, pos - start);
        pos -= start;
    }
    out[pos] = '\0';
}

/* Get the bundles directory path. Returns ~/.slermes/skill-bundles. */
static void bundles_dir(char *out, size_t out_size) {
    const char *override = getenv("HERMES_BUNDLES_DIR");
    if (override && override[0]) {
        snprintf(out, out_size, "%s", override);
        return;
    }
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = ".";
    snprintf(out, out_size, "%s/skill-bundles", home);
}

/* -----------------------------------------------------------------
 *  Parse a single YAML bundle file
 * ================================================================ */

static int load_bundle_file(const char *path, skill_bundle_t *bundle) {
    memset(bundle, 0, sizeof(*bundle));

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &err);
    if (!doc) {
        fprintf(stderr, "Warning: could not parse bundle %s: %s\n", path, err ? err : "unknown");
        free(err);
        return -1;
    }

    /* Name: from YAML "name" field, fallback to filename stem */
    const char *yaml_name = yaml_get_string(doc, "name");
    if (yaml_name && yaml_name[0]) {
        snprintf(bundle->name, sizeof(bundle->name), "%s", yaml_name);
    } else {
        /* Extract filename stem */
        const char *base = strrchr(path, '/');
        base = base ? base + 1 : path;
        char stem[256];
        snprintf(stem, sizeof(stem), "%s", base);
        char *dot = strrchr(stem, '.');
        if (dot) *dot = '\0';
        snprintf(bundle->name, sizeof(bundle->name), "%s", stem);
    }

    /* Slug */
    slugify(bundle->name, bundle->slug, sizeof(bundle->slug));
    if (!bundle->slug[0]) {
        fprintf(stderr, "Warning: bundle %s has empty slug; skipping\n", path);
        yaml_free(doc);
        return -1;
    }

    /* Description */
    const char *desc = yaml_get_string(doc, "description");
    if (desc) snprintf(bundle->description, sizeof(bundle->description), "%s", desc);

    /* Instruction */
    const char *instr = yaml_get_string(doc, "instruction");
    if (instr) snprintf(bundle->instruction, sizeof(bundle->instruction), "%s", instr);

    /* Skills list */
    size_t n = yaml_list_count(doc, "skills");
    if (n == 0) {
        fprintf(stderr, "Warning: bundle %s has no skills; skipping\n", path);
        yaml_free(doc);
        return -1;
    }
    if (n > BUNDLE_SKILLS_MAX) n = BUNDLE_SKILLS_MAX;
    bundle->skill_count = 0;
    for (size_t i = 0; i < n; i++) {
        const char *sk = yaml_list_get(doc, "skills", i);
        if (sk && sk[0]) {
            snprintf(bundle->skills[bundle->skill_count], BUNDLE_SKILL_NAME, "%s", sk);
            bundle->skill_count++;
        }
    }

    if (bundle->skill_count == 0) {
        fprintf(stderr, "Warning: bundle %s has no valid skills; skipping\n", path);
        yaml_free(doc);
        return -1;
    }

    /* Default description */
    if (!bundle->description[0]) {
        snprintf(bundle->description, sizeof(bundle->description),
                 "Load %d skills as a bundle", bundle->skill_count);
    }

    yaml_free(doc);
    return 0;
}

/* -----------------------------------------------------------------
 *  Public API
 * ================================================================ */

int skill_bundles_scan(skill_bundle_registry_t *reg) {
    if (!reg) return -1;
    reg->count = 0;

    char dir[512];
    bundles_dir(dir, sizeof(dir));

    DIR *d = opendir(dir);
    if (!d) return 0; /* No directory yet — not an error */

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && reg->count < BUNDLE_REG_MAX) {
        /* Only .yaml and .yml files */
        const char *name = entry->d_name;
        size_t nlen = strlen(name);
        if (nlen < 6) continue;
        const char *ext = name + nlen - 5;
        if (strcmp(ext, ".yaml") != 0 && strcmp(ext + 1, ".yml") != 0) continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, name);

        /* Skip directories */
        struct stat st;
        if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        skill_bundle_t bundle;
        if (load_bundle_file(full_path, &bundle) == 0) {
            /* Check for duplicate slug */
            bool dup = false;
            for (int i = 0; i < reg->count; i++) {
                if (strcmp(reg->bundles[i].slug, bundle.slug) == 0) {
                    fprintf(stderr, "Warning: duplicate bundle slug '%s' from %s; keeping first\n",
                            bundle.slug, full_path);
                    dup = true;
                    break;
                }
            }
            if (!dup) {
                reg->bundles[reg->count++] = bundle;
            }
        }
    }
    closedir(d);
    return reg->count;
}

const skill_bundle_t *skill_bundle_find(const skill_bundle_registry_t *reg, const char *slug) {
    if (!reg || !slug) return NULL;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->bundles[i].slug, slug) == 0)
            return &reg->bundles[i];
    }
    return NULL;
}

void skill_bundles_print(const skill_bundle_registry_t *reg) {
    if (!reg) return;
    printf("Skill bundles (%d):\n", reg->count);
    for (int i = 0; i < reg->count; i++) {
        const skill_bundle_t *b = &reg->bundles[i];
        printf("  /%s — %s\n", b->slug, b->description);
        printf("    Skills: ");
        for (int j = 0; j < b->skill_count; j++) {
            if (j > 0) printf(", ");
            printf("%s", b->skills[j]);
        }
        printf("\n");
        if (b->instruction[0])
            printf("    Instruction: %s\n", b->instruction);
    }
}
