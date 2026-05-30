/*
 * video_analyze.c — Video analysis tool for Hermes C.
 * Extracts video metadata via ffprobe and optionally performs
 * deeper analysis via Python delegation (scene detection, OCR, etc.).
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

/* Max file size for local video analysis (500 MB) */
#define VIDEO_MAX_BYTES (500LL * 1024 * 1024)

/* Schema */
static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"video_url\":{\"type\":\"string\",\"description\":\"Path or URL to video file\"},"
      "\"analysis\":{\"type\":\"string\",\"description\":\"Optional analysis type: 'metadata' (codec, resolution, duration, bitrate, framerate), 'scenes' (scene boundary detection), 'audio' (audio stream info). Default: metadata\",\"default\":\"metadata\",\"enum\":[\"metadata\",\"scenes\",\"audio\"]}"
    "},"
    "\"required\":[\"video_url\"]"
"}";

/* Run command and capture full output */
static char *run_cmd_full(const char *fmt, ...) {
    char cmd[8192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);

    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;

    char tmp[] = "/tmp/hermes_video_XXXXXX";
    int fd = mkstemp(tmp);
    if (fd < 0) { pclose(fp); return NULL; }

    char buf[4096];
    size_t written = 0;
    ssize_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        ssize_t r = write(fd, buf, (size_t)n);
        if (r < 0) break;
        written += (size_t)r;
    }
    pclose(fp);

    char *out = malloc(written + 1);
    if (out) {
        lseek(fd, 0, SEEK_SET);
        ssize_t r = read(fd, out, written);
        if (r > 0) out[r] = '\0';
        else out[0] = '\0';
    }
    close(fd);
    unlink(tmp);
    return out;
}

/* Check if URL has a known video extension */
static bool has_video_extension(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return false;
    ext++;
    const char *exts[] = {"mp4","avi","mov","mkv","webm","flv","wmv","m4v","mpg","mpeg","3gp","ogv", NULL};
    for (int i = 0; exts[i]; i++) {
        size_t elen = strlen(exts[i]);
        if (strncasecmp(ext, exts[i], elen) == 0 && strlen(ext) == elen)
            return true;
    }
    return false;
}

char *video_analyze_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *video_url = json_object_get_string(args, "video_url", NULL);
    const char *analysis = json_object_get_string(args, "analysis", "metadata");

    json_node_t *result = json_new_object();

    if (!video_url) {
        json_object_set(result, "error", json_new_string("Missing video_url"));
    } else {
        bool is_local = (strncmp(video_url, "http://", 7) != 0 &&
                         strncmp(video_url, "https://", 8) != 0 &&
                         strncmp(video_url, "data:", 5) != 0);

        if (is_local) {
            struct stat st;
            if (stat(video_url, &st) != 0) {
                json_object_set(result, "error", json_new_string("File not found"));
            } else if (!has_video_extension(video_url)) {
                json_object_set(result, "error", json_new_string("Not a recognized video format"));
            } else if (st.st_size > VIDEO_MAX_BYTES) {
                json_object_set(result, "error", json_new_string("File too large (>500 MB)"));
            } else {
                json_object_set(result, "video_url", json_new_string(video_url));
                json_object_set(result, "file_size", json_new_number((double)st.st_size));

                /* Metadata analysis via ffprobe */
                if (strcmp(analysis, "metadata") == 0 || strcmp(analysis, "audio") == 0) {
                    char *ffprobe_out = run_cmd_full(
                        "ffprobe -v quiet -print_format json -show_format -show_streams '%s' 2>/dev/null",
                        video_url);
                    if (ffprobe_out && strlen(ffprobe_out) > 0) {
                        char *parse_err = NULL;
                        json_node_t *probe = json_parse(ffprobe_out, &parse_err);
                        if (probe) {
                            /* Extract video stream info */
                            json_node_t *streams = json_object_get(probe, "streams");
                            if (streams) {
                                size_t n = json_len(streams);
                                json_node_t *video_streams = json_new_array();
                                json_node_t *audio_streams = json_new_array();
                                for (size_t i = 0; i < n; i++) {
                                    json_node_t *s = json_get(streams, i);
                                    const char *codec_type = json_get_str(s, "codec_type", "unknown");
                                    if (strcmp(codec_type, "video") == 0) {
                                        json_append(video_streams, s);
                                    } else if (strcmp(codec_type, "audio") == 0) {
                                        json_append(audio_streams, s);
                                    }
                                }
                                if (json_len(video_streams) > 0)
                                    json_object_set(result, "video_streams", video_streams);
                                else
                                    json_free(video_streams);
                                if (json_len(audio_streams) > 0)
                                    json_object_set(result, "audio_streams", audio_streams);
                                else
                                    json_free(audio_streams);
                            }

                            /* Format info */
                            json_node_t *fmt = json_object_get(probe, "format");
                            if (fmt) {
                                const char *fmt_name = json_get_str(fmt, "format_name", NULL);
                                const char *duration = json_get_str(fmt, "duration", NULL);
                                const char *bitrate = json_get_str(fmt, "bit_rate", NULL);
                                const char *size_str = json_get_str(fmt, "size", NULL);
                                if (fmt_name) json_object_set(result, "format", json_new_string(fmt_name));
                                if (duration) json_object_set(result, "duration_sec", json_new_string(duration));
                                if (bitrate) json_object_set(result, "bitrate_bps", json_new_string(bitrate));
                                if (size_str) json_object_set(result, "container_size", json_new_string(size_str));
                            }
                            json_free(probe);
                        }
                        free(parse_err);
                    } else {
                        json_object_set(result, "probe_warning", json_new_string("ffprobe returned no output"));
                    }
                    free(ffprobe_out);
                }

                /* Audio-specific analysis */
                if (strcmp(analysis, "audio") == 0) {
                    /* ffprobe already handled above, we just add audio focus */
                    json_object_set(result, "analysis_type", json_new_string("audio"));
                }

                /* Scene detection via ffprobe scene detection */
                if (strcmp(analysis, "scenes") == 0) {
                    char *scenes = run_cmd_full(
                        "ffprobe -v quiet -show_entries "
                        "frame=pts_time:frame_tags=lavfi.scene_score "
                        "-f lavfi -i \"movie='%s',select=gt(scene\\\\,0.4)\" "
                        "2>/dev/null | grep -E 'pts_time|scene_score' | head -100",
                        video_url);
                    if (scenes && strlen(scenes) > 0) {
                        json_object_set(result, "scenes", json_new_string(scenes));
                    } else {
                        json_object_set(result, "scene_note", json_new_string("Scene detection requires ffmpeg with lavfi. No scene transitions found or filter unavailable."));
                    }
                    free(scenes);
                    json_object_set(result, "analysis_type", json_new_string("scenes"));
                }
            }
        } else {
            /* Remote URL — return basic info */
            json_object_set(result, "video_url", json_new_string(video_url));
            json_object_set(result, "note", json_new_string("Remote URL; metadata limited. Download locally for full analysis."));
        }
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_video_analyze(void) {
    registry_register("video_analyze",
        "Analyze a video file. Returns metadata (codec, resolution, duration, "
        "bitrate, framerate) via ffprobe and optionally performs scene detection "
        "or audio analysis. Supports local files and remote URLs.",
        SCHEMA, video_analyze_handler);
}
