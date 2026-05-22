/*
 * plugin_spotify.c — Example Spotify integration plugin (P137).
 * Stub showing typed plugin API usage pattern.
 *
 * Build:
 *   gcc -O2 -fPIC -shared -I ../../include -I ../../lib/libplugin \
 *       plugin_spotify.c -o plugin_spotify.so
 */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Plugin metadata
 * ================================================================ */

const char *plugin_meta_name(void) {
    return "spotify-control";
}

const char *plugin_meta_version(void) {
    return "0.3.0";
}

const char *plugin_meta_type(void) {
    return "spotify";
}

const char *plugin_meta_description(void) {
    return "Spotify playback control — play, pause, skip, search";
}

/* No dependencies */
int plugin_deps_count(void) { return 0; }
const plugin_dep_t *plugin_deps_list(void) { return NULL; }

/* ================================================================
 *  Interface function pointers
 * ================================================================ */

static struct {
    bool   connected;
    char   device_id[128];
    char   current_track[256];
    char   current_artist[256];
    bool   playing;
} spotify_state;

static char *spotify_play(const char *uri, const char *device_id) {
    (void)device_id;
    if (!spotify_state.connected)
        return strdup("{\"error\":\"not connected\"}");

    spotify_state.playing = true;
    if (uri) {
        snprintf(spotify_state.current_track, sizeof(spotify_state.current_track), "%s", uri);
        snprintf(spotify_state.current_artist, sizeof(spotify_state.current_artist), "stub-artist");
    }

    char result[256];
    snprintf(result, sizeof(result),
             "{\"status\":\"ok\",\"playing\":true,\"uri\":\"%s\"}",
             uri ? uri : "current");
    return strdup(result);
}

static char *spotify_pause(void) {
    if (!spotify_state.connected)
        return strdup("{\"error\":\"not connected\"}");

    spotify_state.playing = false;
    return strdup("{\"status\":\"ok\",\"playing\":false}");
}

static char *spotify_next(void) {
    if (!spotify_state.connected)
        return strdup("{\"error\":\"not connected\"}");

    snprintf(spotify_state.current_track, sizeof(spotify_state.current_track),
             "stub-next-track");
    return strdup("{\"status\":\"ok\",\"action\":\"next\"}");
}

static char *spotify_current(void) {
    char result[512];
    snprintf(result, sizeof(result),
             "{\"status\":\"ok\",\"track\":\"%s\",\"artist\":\"%s\",\"playing\":%s}",
             spotify_state.current_track, spotify_state.current_artist,
             spotify_state.playing ? "true" : "false");
    return strdup(result);
}

static char *spotify_search(const char *query, const char *type) {
    (void)type;
    char result[512];
    snprintf(result, sizeof(result),
             "{\"results\":[{\"name\":\"%s\",\"artist\":\"stub\",\"uri\":\"spotify:track:stub\"}]}",
             query ? query : "");
    return strdup(result);
}

static plugin_interface_t interface = {
    .type = PLUGIN_SPOTIFY,
    .spotify_play    = spotify_play,
    .spotify_pause   = spotify_pause,
    .spotify_next    = spotify_next,
    .spotify_current = spotify_current,
    .spotify_search  = spotify_search,
};

void *plugin_get_interface(void) {
    return &interface;
}

/* ================================================================
 *  Lifecycle
 * ================================================================ */

int plugin_init(void) {
    fprintf(stderr, "[spotify-control] initializing...\n");
    memset(&spotify_state, 0, sizeof(spotify_state));
    spotify_state.connected = true; /* would connect via Spotify API */
    fprintf(stderr, "[spotify-control] ready\n");
    return 0;
}

int plugin_cleanup(void) {
    fprintf(stderr, "[spotify-control] shutting down...\n");
    spotify_state.connected = false;
    return 0;
}

int plugin_configure(const char *config_json) {
    fprintf(stderr, "[spotify-control] configure: %s\n", config_json);
    /* Parse client_id, client_secret, etc. from config */
    return 0;
}
