/*
 * plugin_spotify.c — Real Spotify Web API plugin (P137).
 *
 * Uses curl via popen() for HTTP calls. No external library dependencies.
 * Supports Client Credentials OAuth flow for token management.
 *
 * Build:
 *   gcc -O2 -fPIC -shared -I ../../include -I ../../lib/libplugin \
 *       -DSPOTIFY_PLUGIN_VERSION=\"1.0.0\" \
 *       plugin_spotify.c -o plugin_spotify.so
 */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ================================================================
 *  Constants
 * ================================================================ */

#define TOKEN_URL   "https://accounts.spotify.com/api/token"
#define API_BASE    "https://api.spotify.com/v1"
#define MAX_STR     4096
#define TOKEN_BUF   2048

/* ================================================================
 *  Plugin metadata
 * ================================================================ */

const char *plugin_meta_name(void) {
    return "spotify-control";
}

const char *plugin_meta_version(void) {
    return SPOTIFY_PLUGIN_VERSION;
}

const char *plugin_meta_type(void) {
    return "spotify";
}

const char *plugin_meta_description(void) {
    return "Spotify playback control via Web API — play, pause, skip, search, current track";
}

/* No dependencies */
int plugin_deps_count(void) { return 0; }
const plugin_dep_t *plugin_deps_list(void) { return NULL; }

/* ================================================================
 *  Internal state
 * ================================================================ */

static struct {
    bool   configured;
    bool   authenticated;
    char   client_id[256];
    char   client_secret[512];
    char   access_token[TOKEN_BUF];
    char   device_id[128];     /* optional target device */
    time_t token_expiry;
    char   last_error[512];
} spotify;

/* ================================================================
 *  Helpers
 * ================================================================ */

/* Run curl command and capture stdout. Returns malloc'd string. */
static char *curl_get(const char *url, const char *auth_header,
                      const char *method, const char *data) {
    char cmd[MAX_STR * 2];
    size_t pos = 0;

    pos += snprintf(cmd + pos, sizeof(cmd) - pos,
        "curl -s -X %s", method ? method : "GET");

    if (auth_header) {
        pos += snprintf(cmd + pos, sizeof(cmd) - pos,
            " -H \"Authorization: %s\"", auth_header);
    }

    /* Content type for POST with data */
    if (data) {
        pos += snprintf(cmd + pos, sizeof(cmd) - pos,
            " -H \"Content-Type: application/json\"");
    }

    /* Device ID for player API */
    if (spotify.device_id[0] && strstr(url, "player")) {
        pos += snprintf(cmd + pos, sizeof(cmd) - pos,
            " -d \"device_id=%s\"", spotify.device_id);
    }

    if (data) {
        pos += snprintf(cmd + pos, sizeof(cmd) - pos,
            " -d '%s'", data);
    }

    pos += snprintf(cmd + pos, sizeof(cmd) - pos, " \"%s\" 2>/dev/null", url);

    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;

    char *buf = (char *)malloc(MAX_STR);
    if (!buf) { pclose(fp); return NULL; }
    buf[0] = '\0';

    size_t total = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (total + len >= MAX_STR - 1) break;
        memcpy(buf + total, line, len);
        total += len;
        buf[total] = '\0';
    }
    pclose(fp);

    /* Truncate trailing newline */
    while (total > 0 && (buf[total - 1] == '\n' || buf[total - 1] == '\r'))
        buf[--total] = '\0';

    return buf;
}

/* Extract string value from JSON key. Returns pointer inside json or NULL. */
static const char *json_strval(const char *json, const char *key) {
    if (!json || !key) return NULL;
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return NULL;
    p = strchr(p + strlen(key) + 2, ':');
    if (!p) return NULL;
    p++;
    while (*p == ' ') p++;
    if (*p != '"') return NULL;
    p++;
    return p; /* Pointer into original json string */
}

/* Extract a bounded string between quotes starting at p */
static size_t quoted_len(const char *p) {
    const char *end = strchr(p, '"');
    return end ? (size_t)(end - p) : 0;
}

/* Check if response has an error field */
static bool has_error(const char *json) {
    return json && strstr(json, "\"error\"") != NULL;
}

/* Refresh access token via Client Credentials flow */
static bool refresh_token(void) {
    if (!spotify.client_id[0] || !spotify.client_secret[0]) {
        snprintf(spotify.last_error, sizeof(spotify.last_error),
                 "spotify not configured: set client_id and client_secret via plugin_configure");
        return false;
    }

    char cmd[MAX_STR];
    snprintf(cmd, sizeof(cmd),
        "curl -s -X POST \"%s\" "
        "-H \"Content-Type: application/x-www-form-urlencoded\" "
        "-d \"grant_type=client_credentials&client_id=%s&client_secret=%s\" 2>/dev/null",
        TOKEN_URL, spotify.client_id, spotify.client_secret);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        snprintf(spotify.last_error, sizeof(spotify.last_error),
                 "failed to run curl for token refresh");
        return false;
    }

    char response[4096] = {0};
    size_t total = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (total + len >= sizeof(response) - 1) break;
        memcpy(response + total, line, len);
        total += len;
        response[total] = '\0';
    }
    int rc = pclose(fp);

    if (rc != 0 || has_error(response)) {
        snprintf(spotify.last_error, sizeof(spotify.last_error),
                 "token refresh failed (curl rc=%d): %.200s", rc, response);
        return false;
    }

    /* Extract access_token */
    const char *tok_start = json_strval(response, "access_token");
    if (!tok_start) {
        snprintf(spotify.last_error, sizeof(spotify.last_error),
                 "no access_token in response");
        return false;
    }
    size_t tlen = quoted_len(tok_start);
    if (tlen == 0 || tlen >= TOKEN_BUF) {
        snprintf(spotify.last_error, sizeof(spotify.last_error),
                 "invalid token length: %zu", tlen);
        return false;
    }
    memcpy(spotify.access_token, tok_start, tlen);
    spotify.access_token[tlen] = '\0';

    /* Extract expires_in */
    const char *exp_start = strstr(response, "\"expires_in\"");
    if (exp_start) {
        exp_start = strchr(exp_start, ':');
        if (exp_start) {
            int expires_in = atoi(exp_start + 1);
            spotify.token_expiry = time(NULL) + expires_in - 60; /* 60s buffer */
        }
    } else {
        spotify.token_expiry = time(NULL) + 3540; /* default ~59 min */
    }

    spotify.authenticated = true;
    return true;
}

/* Ensure we have a valid token, returns "Bearer <token>" string (malloc'd) */
static char *ensure_auth(void) {
    if (!spotify.authenticated || time(NULL) >= spotify.token_expiry) {
        if (!refresh_token()) return NULL;
    }

    size_t len = strlen(spotify.access_token) + 16;
    char *hdr = (char *)malloc(len);
    if (!hdr) return NULL;
    snprintf(hdr, len, "Bearer %s", spotify.access_token);
    return hdr;
}

/* ================================================================
 *  Interface functions
 * ================================================================ */

static char *spotify_play(const char *uri, const char *device_id) {
    (void)device_id; /* use default device */

    char *auth = ensure_auth();
    if (!auth) return strdup("{\"error\":\"authentication failed\"}");

    char url[512];
    snprintf(url, sizeof(url), "%s/me/player/play", API_BASE);

    char *body = NULL;
    if (uri && uri[0]) {
        body = (char *)malloc(MAX_STR);
        if (body) snprintf(body, MAX_STR,
            "{\"uris\":[\"%s\"]}", uri);
    }

    char *result = curl_get(url, auth, "PUT", body);
    free(auth);
    free(body);

    if (!result) return strdup("{\"status\":\"error\",\"message\":\"request failed\"}");

    /* PUT /play returns 204 No Content on success */
    if (result[0] == '\0') {
        free(result);
        return strdup("{\"status\":\"ok\",\"playing\":true}");
    }

    /* Check for error */
    if (has_error(result)) {
        char *err = result;
        result = (char *)malloc(512);
        if (result) snprintf(result, 512, "{\"error\":\"playback failed\",\"detail\":%s}", err);
        free(err);
        return result;
    }

    return result;
}

static char *spotify_pause(void) {
    char *auth = ensure_auth();
    if (!auth) return strdup("{\"error\":\"authentication failed\"}");

    char url[512];
    snprintf(url, sizeof(url), "%s/me/player/pause", API_BASE);

    char *result = curl_get(url, auth, "PUT", NULL);
    free(auth);

    if (!result) return strdup("{\"status\":\"error\",\"message\":\"request failed\"}");

    if (result[0] == '\0') {
        free(result);
        return strdup("{\"status\":\"ok\",\"playing\":false}");
    }

    if (has_error(result)) {
        char *err = result;
        result = (char *)malloc(512);
        if (result) snprintf(result, 512, "{\"error\":\"pause failed\",\"detail\":%s}", err);
        free(err);
        return result;
    }

    return result;
}

static char *spotify_next(void) {
    char *auth = ensure_auth();
    if (!auth) return strdup("{\"error\":\"authentication failed\"}");

    char url[512];
    snprintf(url, sizeof(url), "%s/me/player/next", API_BASE);

    char *result = curl_get(url, auth, "POST", NULL);
    free(auth);

    if (!result) return strdup("{\"status\":\"error\",\"message\":\"request failed\"}");

    if (result[0] == '\0') {
        free(result);
        return strdup("{\"status\":\"ok\",\"action\":\"next\"}");
    }

    if (has_error(result)) {
        char *err = result;
        result = (char *)malloc(512);
        if (result) snprintf(result, 512, "{\"error\":\"next failed\",\"detail\":%s}", err);
        free(err);
        return result;
    }

    return result;
}

static char *spotify_current(void) {
    char *auth = ensure_auth();
    if (!auth) return strdup("{\"error\":\"authentication failed\"}");

    char url[512];
    snprintf(url, sizeof(url), "%s/me/player/currently-playing", API_BASE);

    char *result = curl_get(url, auth, "GET", NULL);
    free(auth);

    if (!result) return strdup("{\"status\":\"error\",\"message\":\"request failed\"}");

    if (result[0] == '\0' || strcmp(result, "{\"error\":{\"status\":204,\"message\":\"\"}}") == 0) {
        free(result);
        return strdup("{\"status\":\"ok\",\"playing\":false,\"message\":\"no active playback\"}");
    }

    /* Parse and re-format a clean response */
    const char *item_start = strstr(result, "\"item\"");
    const char *name_start = NULL;
    char track_name[512] = "unknown";
    char artist_name[256] = "unknown";

    if (item_start) {
        /* Extract track name from item.name */
        name_start = json_strval(item_start, "name");
        if (name_start) {
            size_t nlen = quoted_len(name_start);
            if (nlen > 0 && nlen < sizeof(track_name)) {
                memcpy(track_name, name_start, nlen);
                track_name[nlen] = '\0';
            }
        }
        /* Extract first artist name from item.artists[0].name */
        const char *artists_start = strstr(item_start, "\"artists\"");
        if (artists_start) {
            const char *first_artist = strchr(artists_start + 9, '{');
            if (first_artist) {
                name_start = json_strval(first_artist, "name");
                if (name_start) {
                    size_t alen = quoted_len(name_start);
                    if (alen > 0 && alen < sizeof(artist_name)) {
                        memcpy(artist_name, name_start, alen);
                        artist_name[alen] = '\0';
                    }
                }
            }
        }
    }

    /* Get playing state */
    bool is_playing = strstr(result, "\"is_playing\":true") != NULL;

    free(result);

    char *out = (char *)malloc(1024);
    if (!out) return strdup("{\"error\":\"out of memory\"}");
    snprintf(out, 1024,
        "{\"status\":\"ok\",\"track\":\"%s\",\"artist\":\"%s\",\"playing\":%s}",
        track_name, artist_name, is_playing ? "true" : "false");
    return out;
}

static char *spotify_search(const char *query, const char *type) {
    if (!query || !query[0])
        return strdup("{\"error\":\"query required\"}");

    if (!type) type = "track";

    char *auth = ensure_auth();
    if (!auth) return strdup("{\"error\":\"authentication failed\"}");

    char url[MAX_STR];
    snprintf(url, sizeof(url), "%s/search?q=%s&type=%s&limit=5",
             API_BASE, query, type);

    char *result = curl_get(url, auth, "GET", NULL);
    free(auth);

    if (!result) return strdup("{\"error\":\"search request failed\"}");

    if (has_error(result)) {
        char *err = result;
        result = (char *)malloc(512);
        if (result) snprintf(result, 512, "{\"error\":\"search failed\",\"detail\":%s}", err);
        free(err);
        return result;
    }

    /* Simplify response to just names + artists */
    char *out = (char *)malloc(MAX_STR);
    if (!out) { free(result); return strdup("{\"error\":\"out of memory\"}"); }

    size_t pos = 0;
    pos += snprintf(out + pos, MAX_STR - pos,
        "{\"results\":[");

    /* Find items array for the requested type */
    char type_key[32];
    snprintf(type_key, sizeof(type_key), "\"%ss\"", type);
    const char *items = strstr(result, type_key);
    if (!items) items = strstr(result, "\"tracks\""); /* fallback */

    if (items) {
        items = strchr(items, '[');
        if (items) {
            int count = 0;
            const char *p = items;
            while ((p = strstr(p, "{\"id\"")) && count < 5) {
                const char *n = json_strval(p, "name");
                const char *a_start = strstr(p, "\"artists\"");
                const char *a = NULL;
                if (a_start) {
                    const char *first = strchr(a_start + 9, '{');
                    if (first) a = json_strval(first, "name");
                }

                char name_buf[256] = "?";
                char artist_buf[128] = "?";
                if (n) { size_t nl = quoted_len(n); if (nl > 0 && nl < sizeof(name_buf)) { memcpy(name_buf, n, nl); name_buf[nl] = '\0'; } }
                if (a) { size_t al = quoted_len(a); if (al > 0 && al < sizeof(artist_buf)) { memcpy(artist_buf, a, al); artist_buf[al] = '\0'; } }

                if (count > 0) out[pos++] = ',';
                pos += snprintf(out + pos, MAX_STR - pos,
                    "{\"name\":\"%s\",\"artist\":\"%s\"}",
                    name_buf, artist_buf);
                count++;
                p++;
            }
        }
    }

    pos += snprintf(out + pos, MAX_STR - pos, "]}");
    free(result);
    return out;
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
    fprintf(stderr, "[spotify-control] initializing v%s...\n", SPOTIFY_PLUGIN_VERSION);
    memset(&spotify, 0, sizeof(spotify));
    fprintf(stderr, "[spotify-control] ready — use plugin_configure() with client_id and client_secret\n");
    return 0;
}

int plugin_cleanup(void) {
    fprintf(stderr, "[spotify-control] shutting down...\n");
    memset(&spotify, 0, sizeof(spotify));
    return 0;
}

int plugin_configure(const char *config_json) {
    fprintf(stderr, "[spotify-control] configure: %s\n", config_json);
    if (!config_json) return 0;

    /* Extract client_id */
    const char *cid = json_strval(config_json, "client_id");
    if (cid) {
        size_t clen = quoted_len(cid);
        if (clen > 0 && clen < sizeof(spotify.client_id)) {
            memcpy(spotify.client_id, cid, clen);
            spotify.client_id[clen] = '\0';
        }
    }

    /* Extract client_secret */
    const char *cs = json_strval(config_json, "client_secret");
    if (cs) {
        size_t slen = quoted_len(cs);
        if (slen > 0 && slen < sizeof(spotify.client_secret)) {
            memcpy(spotify.client_secret, cs, slen);
            spotify.client_secret[slen] = '\0';
        }
    }

    /* Extract device_id (optional) */
    const char *did = json_strval(config_json, "device_id");
    if (did) {
        size_t dlen = quoted_len(did);
        if (dlen > 0 && dlen < sizeof(spotify.device_id)) {
            memcpy(spotify.device_id, did, dlen);
            spotify.device_id[dlen] = '\0';
        }
    }

    spotify.configured = spotify.client_id[0] && spotify.client_secret[0];
    if (spotify.configured) {
        fprintf(stderr, "[spotify-control] configured with client_id=%s\n", spotify.client_id);
    }
    return 0;
}
