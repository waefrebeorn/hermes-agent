/*
 * media_cache.c — Local media file cache for Hermes C.
 * Saves images/audio/video bytes to ~/.hermes/cache/<type>/ with random
 * filenames. Provides cleanup of expired files.
 * Mirrors Python gateway/platforms/base.py cache functions.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/* Get the HERMES_HOME directory path (static buffer, not thread-safe). */
static const char *get_hermes_home_dir(void) {
    static char buf[1024];
    const char *home = getenv("HERMES_HOME");
    if (home && home[0]) {
        snprintf(buf, sizeof(buf), "%s", home);
        return buf;
    }
    home = getenv("HOME");
    if (home && home[0]) {
        snprintf(buf, sizeof(buf), "%s/.hermes", home);
        return buf;
    }
    return "/tmp/.hermes";
}

/* Ensure a directory exists (mkdir -p equivalent for single level). */
static bool ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return true;
    return mkdir(path, 0755) == 0;
}

/* Generate a random hex string into buf (len must be even, max 64).
 * Uses /dev/urandom on Linux. */
static void random_hex(char *buf, int len) {
    const char *hex = "0123456789abcdef";
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        /* Fallback: use time + pid */
        snprintf(buf, (size_t)len + 1, "%lx%x", (unsigned long)time(NULL), getpid());
        return;
    }
    unsigned char rand_bytes[32];
    int want = len / 2;
    if (want > 32) want = 32;
    ssize_t n = read(fd, rand_bytes, (size_t)want);
    close(fd);
    if (n < 1) {
        snprintf(buf, (size_t)len + 1, "%lx%x", (unsigned long)time(NULL), getpid());
        return;
    }
    for (int i = 0; i < (int)n && i < len; i++) {
        buf[i * 2] = hex[rand_bytes[i] >> 4];
        buf[i * 2 + 1] = hex[rand_bytes[i] & 0x0f];
    }
    buf[(int)n * 2] = '\0';
}

/* Check if data starts with a known image magic-byte sequence.
 * Mirrors Python base.py _looks_like_image(). */
static bool looks_like_image(const unsigned char *data, size_t len) {
    if (!data || len < 4) return false;
    /* PNG: 89 50 4E 47 0D 0A 1A 0A */
    if (len >= 8 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' &&
        data[3] == 'G' && data[4] == 0x0D && data[5] == 0x0A &&
        data[6] == 0x1A && data[7] == 0x0A)
        return true;
    /* JPEG: FF D8 FF */
    if (len >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
        return true;
    /* GIF: GIF87a or GIF89a */
    if (len >= 6 && data[0] == 'G' && data[1] == 'I' && data[2] == 'F' &&
        (data[3] == '8' || data[3] == '9') && data[4] == '7' && data[5] == 'a')
        return true;
    /* BMP: BM */
    if (len >= 2 && data[0] == 'B' && data[1] == 'M')
        return true;
    /* WebP: RIFF .... WEBP */
    if (len >= 12 && data[0] == 'R' && data[1] == 'I' && data[2] == 'F' &&
        data[3] == 'F' && data[8] == 'W' && data[9] == 'E' &&
        data[10] == 'B' && data[11] == 'P')
        return true;
    return false;
}

/* ================================================================
 *  Public API
 * ================================================================ */

/* Save raw data to the media cache and return the absolute path.
 * type: "images", "audio", or "videos" (creates subdirectory under cache).
 * data: raw bytes to save.
 * data_len: length of data.
 * ext: file extension including dot (e.g. ".jpg", ".ogg").
 * is_image: if true, validates magic bytes before saving.
 * Returns malloc'd path string (caller must free) or NULL on error.
 * Mirrors Python base.py cache_image_from_bytes() / cache_audio_from_bytes(). */
char *media_cache_save(const char *type, const unsigned char *data, size_t data_len,
                        const char *ext, bool is_image) {
    if (!type || !*type || !data || data_len == 0 || !ext || !*ext)
        return NULL;

    /* Validate image magic bytes if requested */
    if (is_image && !looks_like_image(data, data_len))
        return NULL;

    /* Build cache directory path: HERMES_HOME/cache/<type>/ */
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/cache/%s", get_hermes_home_dir(), type);
    if (!ensure_dir(dir)) return NULL;

    /* Ensure parent "cache" directory exists too */
    char cache_dir[1024];
    snprintf(cache_dir, sizeof(cache_dir), "%s/cache", get_hermes_home_dir());
    ensure_dir(cache_dir);
    if (!ensure_dir(dir)) return NULL;

    /* Generate random filename: img_<12 hex chars><ext> */
    char rand_hex[13];
    random_hex(rand_hex, 6); /* 6 bytes → 12 hex chars */

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s_%s%s", dir,
             is_image ? "img" : (strcmp(type, "audio") == 0 ? "audio" : "video"),
             rand_hex, ext);

    /* Write bytes to file */
    FILE *f = fopen(path, "wb");
    if (!f) return NULL;
    size_t written = fwrite(data, 1, data_len, f);
    fclose(f);

    if (written != data_len) {
        unlink(path);
        return NULL;
    }

    return strdup(path);
}

/* Delete cached files in type directory older than max_age_hours.
 * type: "images", "audio", or "videos".
 * max_age_hours: age threshold in hours.
 * Returns number of files removed.
 * Mirrors Python base.py cleanup_image_cache(). */
int media_cache_cleanup(const char *type, int max_age_hours) {
    if (!type || !*type || max_age_hours <= 0) return 0;

    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/cache/%s", get_hermes_home_dir(), type);

    struct stat dir_stat;
    if (stat(dir, &dir_stat) != 0 || !S_ISDIR(dir_stat.st_mode))
        return 0;

    time_t cutoff = time(NULL) - (time_t)max_age_hours * 3600;
    int removed = 0;

    DIR *d = opendir(dir);
    if (!d) return 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue; /* skip . and .. */

        /* Build full path */
        char fpath[1024];
        snprintf(fpath, sizeof(fpath), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (stat(fpath, &st) == 0 && S_ISREG(st.st_mode)) {
            if (st.st_mtime < cutoff) {
                if (unlink(fpath) == 0)
                    removed++;
            }
        }
    }
    closedir(d);
    return removed;
}

/* ================================================================
 *  Audio Routing Helper
 * ================================================================ */

/* Known audio file extensions */
static const char *AUDIO_EXTS[] = {".ogg", ".opus", ".mp3", ".wav", ".m4a", ".flac", NULL};

/* Telegram Bot API sendAudio accepts only MP3 / M4A */
static const char *TELEGRAM_AUDIO_ATTACHMENT_EXTS[] = {".mp3", ".m4a", NULL};

/* Telegram Bot API sendVoice accepts Opus/OGG */
static const char *TELEGRAM_VOICE_EXTS[] = {".ogg", ".opus", NULL};

/* Check if extension is in a list */
static bool ext_in_list(const char *ext, const char **list) {
    for (int i = 0; list[i]; i++) {
        if (strcasecmp(ext, list[i]) == 0) return true;
    }
    return false;
}

/* Determine if a media file should be sent as audio (voice message).
 * For Telegram: Opus/OGG only when is_voice=true, MP3/M4A always audio.
 * Other platforms: all known audio extensions are sent as audio.
 * Returns true if the file should use the audio delivery path.
 * Mirrors Python base.py should_send_media_as_audio(). */
bool media_should_send_as_audio(const char *ext, bool is_telegram, bool is_voice) {
    if (!ext || !*ext) return false;
    if (!ext_in_list(ext, AUDIO_EXTS)) return false;
    if (is_telegram) {
        if (ext_in_list(ext, TELEGRAM_VOICE_EXTS))
            return is_voice;
        return ext_in_list(ext, TELEGRAM_AUDIO_ATTACHMENT_EXTS);
    }
    return true;
}
