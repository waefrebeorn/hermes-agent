/* Test spotify plugin via plugin API */
#include "plugin.h"
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
    printf("=== Plugin Spotify Tests ===\n");

    /* Load the plugin */
    const char *plugin_path = PLUGIN_DIR "/plugin_spotify.so";
    plugin_t *p = plugin_load(plugin_path);
    TEST("plugin loads", p != NULL);
    if (!p) {
        printf("  Error: %s (path: %s)\n", plugin_error(), plugin_path);
        printf("\n=== %d/%d passed ===\n", pass, pass + fail);
        return 1;
    }

    /* Check metadata */
    TEST("name = spotify-control", strcmp(plugin_name(p), "spotify-control") == 0);
    TEST("type = SPOTIFY", plugin_type(p) == PLUGIN_SPOTIFY);
    const char *desc = plugin_description(p);
    TEST("description not null", desc != NULL);
    TEST("description mentions Spotify", desc && strstr(desc, "Spotify") != NULL);

    /* Check version */
    const plugin_version_t *ver = plugin_version(p);
    TEST("version not null", ver != NULL);
    TEST("major == 1", ver && ver->major == 1);

    /* Check deps */
    int dep_count;
    const plugin_dep_t *deps = plugin_deps(p, &dep_count);
    TEST("deps accessible", deps != NULL);
    TEST("no dependencies", dep_count == 0);

    /* Init */
    {
        typedef int (*init_fn_t)(void);
        init_fn_t init_fn = (init_fn_t)plugin_symbol(p, "plugin_init");
        TEST("plugin_init symbol found", init_fn != NULL);
        if (init_fn) {
            int rc = init_fn();
            TEST("init returns 0", rc == 0);
        }
    }

    /* Get interface */
    plugin_interface_t *iface = NULL;
    {
        void *(*get_iface)(void) = (void *(*)(void))plugin_symbol(p, "plugin_get_interface");
        TEST("plugin_get_interface found", get_iface != NULL);
        if (get_iface) {
            iface = (plugin_interface_t *)get_iface();
            TEST("interface not null", iface != NULL);
            TEST("interface type = SPOTIFY", iface != NULL && iface->type == PLUGIN_SPOTIFY);
        }
    }

    if (iface) {
        /* Check all 5 function pointers are present */
        TEST("spotify_play not null", iface->spotify_play != NULL);
        TEST("spotify_pause not null", iface->spotify_pause != NULL);
        TEST("spotify_next not null", iface->spotify_next != NULL);
        TEST("spotify_current not null", iface->spotify_current != NULL);
        TEST("spotify_search not null", iface->spotify_search != NULL);

        /* Call search without auth — should return error gracefully */
        char *r = iface->spotify_search("test", "track");
        TEST("search without auth returns result", r != NULL);
        TEST("search without auth returns error", r && strstr(r, "authentication failed") != NULL);
        if (r) { printf("  (expected — no credentials): %.80s\n", r); free(r); }

        /* Call current without auth — should return error gracefully */
        r = iface->spotify_current();
        TEST("current without auth returns result", r != NULL);
        TEST("current without auth returns error", r && strstr(r, "authentication failed") != NULL);
        free(r);

        /* Call play without auth — should return error gracefully */
        r = iface->spotify_play("spotify:track:test", NULL);
        TEST("play without auth returns result", r != NULL);
        TEST("play without auth returns error", r && strstr(r, "authentication failed") != NULL);
        free(r);

        /* Test configure */
        typedef int (*cfg_fn_t)(const char *);
        cfg_fn_t cfg_fn = (cfg_fn_t)plugin_symbol(p, "plugin_configure");
        TEST("plugin_configure symbol found", cfg_fn != NULL);
        if (cfg_fn) {
            int rc = cfg_fn("{\"client_id\":\"test_id\",\"client_secret\":\"test_secret\",\"device_id\":\"test_device\"}");
            TEST("configure returns 0", rc == 0);

            /* Now search should try to auth (will fail because credentials are fake) */
            r = iface->spotify_search("test", "track");
            TEST("search after configure returns result", r != NULL);
            /* Should either error with auth failure or curl failure */
            if (r) { printf("  (configured, will attempt real auth): %.100s\n", r); free(r); }
        }
    } else {
        printf("  FAIL: interface functions missing\n");
        fail++;
    }

    /* Cleanup */
    {
        typedef int (*cleanup_fn_t)(void);
        cleanup_fn_t cleanup_fn = (cleanup_fn_t)plugin_symbol(p, "plugin_cleanup");
        TEST("plugin_cleanup found", cleanup_fn != NULL);
        if (cleanup_fn) {
            int rc = cleanup_fn();
            TEST("cleanup returns 0", rc == 0);
        }
    }

    plugin_unload(p);

    printf("\n=== %d/%d passed, %d failed ===\n", pass, pass + fail, fail);
    return fail > 0 ? 1 : 0;
}
