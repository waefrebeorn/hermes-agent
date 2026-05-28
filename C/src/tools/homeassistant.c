/*
 * homeassistant.c — HomeAssistant tools for Hermes C.
 * REST API wrappers for HA instance control.
 * Requires HASS_TOKEN + HASS_URL env vars.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Helpers
 * ================================================================ */

/* Get HA base URL from env */
static const char *ha_base(void) {
    const char *url = getenv("HASS_URL");
    return url ? url : "http://homeassistant.local:8123";
}

/* Get HA auth header */
static void ha_auth_hdr(char *buf, size_t sz) {
    const char *token = getenv("HASS_TOKEN");
    if (token && *token)
        snprintf(buf, sz, "Authorization: Bearer %s", token);
    else
        buf[0] = '\0';
}

/* Build HA API URL */
static void ha_url(char *buf, size_t sz, const char *path) {
    snprintf(buf, sz, "%s/api%s", ha_base(), path);
}

/* Check if HA is configured */
static const char *ha_check(void) {
    const char *token = getenv("HASS_TOKEN");
    if (!token || !*token)
        return "HASS_TOKEN not set";
    return NULL; /* OK */
}

/* Validate HA service/domain name — lowercase alphanumeric + underscore only */
static int ha_valid_name(const char *name) {
    if (!name || !*name) return 0;
    for (const char *p = name; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') ||
              *p == '_'))
            return 0;
    }
    return 1;
}

/* Blocked service domains — arbitrary code execution prevention */
static int ha_blocked_domain(const char *domain) {
    if (!domain) return 0;
    static const char *blocked[] = {
        "shell_command", "command_line", "python_script",
        "pyscript", "hassio", "rest_command", NULL
    };
    for (int i = 0; blocked[i]; i++) {
        if (strcmp(domain, blocked[i]) == 0) return 1;
    }
    return 0;
}

/* Make HA GET request, return raw wrapped response */
static char *ha_get_json(const char *api_path) {
    const char *err = ha_check();
    if (err) {
        char resp[512];
        snprintf(resp, sizeof(resp), "{\"success\":false,\"error\":\"%s\"}", err);
        return strdup(resp);
    }

    http_t *h = http_new(15);
    if (!h) return strdup("{\"success\":false,\"error\":\"OOM\"}");

    char auth[256];
    ha_auth_hdr(auth, sizeof(auth));

    char url[1024];
    ha_url(url, sizeof(url), api_path);

    http_resp_t *r = http_get(h, url, auth);
    if (!r) {
        http_free(h);
        return strdup("{\"success\":false,\"error\":\"HTTP request failed\"}");
    }

    char *json = NULL;
    if (r->status == 200 && r->body) {
        size_t sz = strlen(r->body) + 128;
        json = (char *)malloc(sz);
        if (json) snprintf(json, sz,
            "{\"success\":true,\"data\":%s}", r->body);
    } else {
        json = (char *)malloc(512);
        if (json) snprintf(json, 512,
            "{\"success\":false,\"error\":\"HA returned HTTP %d\"}", r->status);
    }

    http_resp_free(r);
    http_free(h);
    return json ? json : strdup("{\"success\":false,\"error\":\"OOM\"}");
}

/* Fetch /api/states, filter by domain/area, return compact summary */
static char *ha_list_filtered(const char *domain, const char *area) {
    const char *err = ha_check();
    if (err) {
        char resp[512];
        snprintf(resp, sizeof(resp), "{\"success\":false,\"error\":\"%s\"}", err);
        return strdup(resp);
    }

    http_t *h = http_new(15);
    if (!h) return strdup("{\"success\":false,\"error\":\"OOM\"}");

    char auth[256];
    ha_auth_hdr(auth, sizeof(auth));
    char url[1024];
    ha_url(url, sizeof(url), "/states");

    http_resp_t *r = http_get(h, url, auth);
    if (!r) {
        http_free(h);
        return strdup("{\"success\":false,\"error\":\"HTTP request failed\"}");
    }

    char *result = NULL;
    if (r->status == 200 && r->body) {
        json_t *arr = json_parse(r->body, NULL);
        if (!arr || arr->type != JSON_ARRAY) {
            result = strdup("{\"success\":false,\"error\":\"Failed to parse states\"}");
        } else {
            /* Build filtered list */
            char buf[16384];
            int len = snprintf(buf, sizeof(buf), "{\"success\":true,\"data\":[");
            int first = 1;

            for (size_t idx = 0; idx < arr->c.count; idx++) {
                json_t *item = arr->c.items[idx];
                if (!item || item->type != JSON_OBJECT) continue;
                const char *eid = json_get_str(item, "entity_id", "");

                /* Filter by domain: entity_id starts with "domain." */
                if (domain && *domain) {
                    size_t dlen = strlen(domain);
                    if (strncmp(eid, domain, dlen) != 0 || eid[dlen] != '.')
                        continue;
                }

                /* Filter by area: check 'area' attribute or friendly_name */
                if (area && *area) {
                    json_t *attrs = json_obj_get(item, "attributes");
                    const char *friendly = "";
                    const char *entity_area = "";
                    if (attrs && attrs->type == JSON_OBJECT) {
                        friendly = json_get_str(attrs, "friendly_name", "");
                        entity_area = json_get_str(attrs, "area", "");
                    }
                    if (!strstr(friendly, area) && !strstr(entity_area, area))
                        continue;
                }

                /* Build compact entity entry */
                const char *state = json_get_str(item, "state", "unknown");
                json_t *attrs = json_obj_get(item, "attributes");
                const char *friendly = "";
                if (attrs && attrs->type == JSON_OBJECT)
                    friendly = json_get_str(attrs, "friendly_name", "");
                const char *last_changed = json_get_str(item, "last_changed", "");

                int n = snprintf(buf + len, sizeof(buf) - len,
                    "%s{\"entity_id\":\"%s\",\"state\":\"%s\",\"friendly_name\":\"%s\"",
                    first ? "" : ",",
                    eid, state, friendly);
                len += n;
                if (last_changed && *last_changed) {
                    n = snprintf(buf + len, sizeof(buf) - len,
                        ",\"last_changed\":\"%s\"", last_changed);
                    len += n;
                }
                n = snprintf(buf + len, sizeof(buf) - len, "}");
                len += n;
                first = 0;

                if (len > (int)sizeof(buf) - 512) break;
            }

            snprintf(buf + len, sizeof(buf) - len, "]}");
            result = strdup(buf);
        }
        if (arr) json_free(arr);
    } else {
        result = (char *)malloc(512);
        if (result) snprintf(result, 512,
            "{\"success\":false,\"error\":\"HA returned HTTP %d\"}", r->status);
    }

    http_resp_free(r);
    http_free(h);
    return result ? result : strdup("{\"success\":false,\"error\":\"OOM\"}");
}

/* ha_list_entities: List all entities (GET /api/states), optional domain/area filter */
char *ha_list_entities_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return ha_get_json("/states");

    const char *domain = json_get_str(args, "domain", "");
    const char *area = json_get_str(args, "area", "");

    char *result;
    if (*domain || *area) {
        result = ha_list_filtered(*domain ? domain : NULL,
                                   *area ? area : NULL);
    } else {
        result = ha_get_json("/states");
    }

    json_free(args);
    return result;
}

/* ha_get_state: Get a single entity state (GET /api/states/<entity_id>) */
char *ha_get_state_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"success\":false,\"error\":\"Invalid JSON\"}");

    const char *entity_id = json_get_str(args, "entity_id", "");
    if (!entity_id || !*entity_id) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"Missing 'entity_id' parameter\"}");
    }

    char path[512];
    snprintf(path, sizeof(path), "/states/%s", entity_id);
    json_free(args);
    return ha_get_json(path);
}

/* ha_list_services: List all services (GET /api/services), optional domain filter */
char *ha_list_services_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return ha_get_json("/services");

    const char *domain = json_get_str(args, "domain", "");
    json_free(args);

    if (!*domain) return ha_get_json("/services");

    /* Fetch all services, filter by domain */
    const char *err = ha_check();
    if (err) {
        char resp[512];
        snprintf(resp, sizeof(resp), "{\"success\":false,\"error\":\"%s\"}", err);
        return strdup(resp);
    }

    http_t *h = http_new(15);
    if (!h) return strdup("{\"success\":false,\"error\":\"OOM\"}");

    char auth[256];
    ha_auth_hdr(auth, sizeof(auth));
    char url[1024];
    ha_url(url, sizeof(url), "/services");

    http_resp_t *r = http_get(h, url, auth);
    char *result = NULL;

    if (r && r->status == 200 && r->body) {
        json_t *arr = json_parse(r->body, NULL);
        if (arr && arr->type == JSON_ARRAY) {
            char buf[4096];
            int len = snprintf(buf, sizeof(buf), "{\"success\":true,\"data\":[");
            int first = 1;

            for (size_t idx = 0; idx < arr->c.count; idx++) {
                json_t *item = arr->c.items[idx];
                if (!item || item->type != JSON_OBJECT) continue;
                const char *d = json_get_str(item, "domain", "");
                if (strcmp(d, domain) != 0) continue;

                int n = snprintf(buf + len, sizeof(buf) - len,
                    "%s{\"domain\":\"%s\"}", first ? "" : ",", d);
                len += n;
                first = 0;
                if (len > (int)sizeof(buf) - 256) break;
            }

            snprintf(buf + len, sizeof(buf) - len, "]}");
            result = strdup(buf);
        }
        if (arr) json_free(arr);
    } else if (r) {
        result = (char *)malloc(512);
        if (result) snprintf(result, 512,
            "{\"success\":false,\"error\":\"HA returned HTTP %d\"}", r->status);
    } else {
        result = strdup("{\"success\":false,\"error\":\"HTTP request failed\"}");
    }

    if (r) http_resp_free(r);
    http_free(h);
    return result ? result : strdup("{\"success\":false,\"error\":\"OOM\"}");
}

/* Build call_service payload JSON string: merge entity_id + data/service_data */
static char *ha_build_payload(const char *entity_id, json_t *args) {
    /* Start with empty object { } */
    json_t *payload = json_object();
    if (!payload) return NULL;

    /* Add entity_id if provided */
    if (entity_id && *entity_id) {
        json_t *eid_node = json_string(entity_id);
        if (eid_node) json_set(payload, "entity_id", eid_node);
    }

    /* Try 'data' as JSON object first */
    json_t *data_obj = json_obj_get(args, "data");
    if (data_obj && data_obj->type == JSON_OBJECT) {
        /* Copy all keys from data object into payload */
        for (size_t i = 0; i < data_obj->c.count; i++) {
            const char *key = data_obj->c.keys[i];
            json_t *val = data_obj->c.items[i];
            if (!key || !val) continue;
            if (strcmp(key, "entity_id") == 0) continue; /* entity_id separately */
            json_t *copy = json_copy(val);
            if (copy) json_set(payload, key, copy);
        }
    } else {
        /* Fallback: try 'service_data' as a JSON string */
        const char *sd_str = json_get_str(args, "service_data", "");
        if (*sd_str) {
            json_t *sd = json_parse(sd_str, NULL);
            if (sd && sd->type == JSON_OBJECT) {
                for (size_t i = 0; i < sd->c.count; i++) {
                    const char *key = sd->c.keys[i];
                    json_t *val = sd->c.items[i];
                    if (!key || !val) continue;
                    if (strcmp(key, "entity_id") == 0) continue;
                    json_t *copy = json_copy(val);
                    if (copy) json_set(payload, key, copy);
                }
            }
            if (sd) json_free(sd);
        }
    }

    char *result = json_serialize(payload);
    json_free(payload);
    return result;
}

/* ha_call_service: Call a service (POST /api/services/<domain>/<service>) */
char *ha_call_service_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    const char *err = ha_check();
    if (err) {
        char resp[512];
        snprintf(resp, sizeof(resp), "{\"success\":false,\"error\":\"%s\"}", err);
        return strdup(resp);
    }

    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"success\":false,\"error\":\"Invalid JSON\"}");

    const char *domain = json_get_str(args, "domain", "");
    const char *service = json_get_str(args, "service", "");

    if (!domain || !*domain || !service || !*service) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"Missing 'domain' or 'service' parameter\"}");
    }

    /* Validate domain/service names (security: prevent path traversal) */
    if (!ha_valid_name(domain)) {
        json_free(args);
        char resp[512];
        snprintf(resp, sizeof(resp),
                 "{\"success\":false,\"error\":\"Invalid domain format: %s\"}", domain);
        return strdup(resp);
    }
    if (!ha_valid_name(service)) {
        json_free(args);
        char resp[512];
        snprintf(resp, sizeof(resp),
                 "{\"success\":false,\"error\":\"Invalid service format: %s\"}", service);
        return strdup(resp);
    }

    /* Check blocked domains */
    if (ha_blocked_domain(domain)) {
        json_free(args);
        return strdup(
            "{\"success\":false,\"error\":\"Service domain is blocked for security "
            "(shell_command, command_line, python_script, pyscript, hassio, rest_command)\"}");
    }

    const char *entity_id = json_get_str(args, "entity_id", "");

    char *payload_str = ha_build_payload(entity_id, args);
    if (!payload_str) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"OOM\"}");
    }

    http_t *h = http_new(15);
    if (!h) {
        free(payload_str);
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"OOM\"}");
    }

    char auth[256];
    ha_auth_hdr(auth, sizeof(auth));

    char url[1024];
    snprintf(url, sizeof(url), "%s/api/services/%s/%s", ha_base(), domain, service);

    http_resp_t *resp = http_post_json_auth(h, url, payload_str, auth);

    char *json = NULL;
    if (resp && resp->body) {
        size_t sz = strlen(resp->body) + 128;
        json = (char *)malloc(sz);
        if (json) {
            if (resp->status == 200)
                snprintf(json, sz, "{\"success\":true,\"data\":%s}", resp->body);
            else
                snprintf(json, sz, "{\"success\":false,\"error\":\"HA returned HTTP %d\"}", resp->status);
        }
    } else {
        json = strdup("{\"success\":false,\"error\":\"HTTP request failed\"}");
    }

    if (resp) http_resp_free(resp);
    http_free(h);
    free(payload_str);
    json_free(args);
    return json ? json : strdup("{\"success\":false,\"error\":\"OOM\"}");
}

/* ha_get_history: Get state history for an entity (GET /api/history/period/<entity_id>) */
char *ha_get_history_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"success\":false,\"error\":\"Invalid JSON\"}");

    const char *entity_id = json_get_str(args, "entity_id", "");
    if (!entity_id || !*entity_id) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"Missing 'entity_id' parameter\"}");
    }

    /* Optional: start_time (ISO format), end_time */
    const char *start_time = json_get_str(args, "start_time", "");
    const char *end_time = json_get_str(args, "end_time", "");

    char path[1024];
    if (*start_time)
        snprintf(path, sizeof(path), "/history/period/%s?filter_entity_id=%s", start_time, entity_id);
    else
        snprintf(path, sizeof(path), "/history/period/%s", entity_id);
    if (*end_time) {
        size_t plen = strlen(path);
        snprintf(path + plen, sizeof(path) - plen, "&end_time=%s", end_time);
    }

    json_free(args);
    return ha_get_json(path);
}

/* ================================================================
 *  Registration
 * ================================================================ */

void registry_init_homeassistant(void) {
    registry_register("ha_list_entities",
        "List HomeAssistant entities. Optionally filter by domain (light, switch, "
        "climate, sensor, etc.) or area name (living room, kitchen, etc.). "
        "Requires HASS_TOKEN.",
        "{\"type\":\"object\",\"properties\":{"
          "\"domain\":{\"type\":\"string\",\"description\":\"Entity domain to filter by (e.g., light, switch, climate). Omit to list all.\"},"
          "\"area\":{\"type\":\"string\",\"description\":\"Area/room name to filter by (e.g., living room, kitchen). Matches entity friendly names.\"}"
        "},\"required\":[]}",
        ha_list_entities_handler);

    registry_register("ha_get_state",
        "Get the state of a specific HomeAssistant entity with all attributes. "
        "Requires HASS_TOKEN.",
        "{\"type\":\"object\",\"properties\":{"
          "\"entity_id\":{\"type\":\"string\",\"description\":\"Entity ID (e.g., light.living_room)\"}"
        "},\"required\":[\"entity_id\"]}",
        ha_get_state_handler);

    registry_register("ha_list_services",
        "List available HomeAssistant services (actions). "
        "Optionally filter by domain. Requires HASS_TOKEN.",
        "{\"type\":\"object\",\"properties\":{"
          "\"domain\":{\"type\":\"string\",\"description\":\"Filter by domain (e.g., light, climate). Omit to list all.\"}"
        "},\"required\":[]}",
        ha_list_services_handler);

    registry_register("ha_call_service",
        "Call a HomeAssistant service to control a device. "
        "Use ha_list_services to discover available services. "
        "Requires HASS_TOKEN.",
        "{\"type\":\"object\",\"properties\":{"
          "\"domain\":{\"type\":\"string\",\"description\":\"Service domain (e.g., light, switch, climate)\"},"
          "\"service\":{\"type\":\"string\",\"description\":\"Service name (e.g., turn_on, turn_off, toggle)\"},"
          "\"entity_id\":{\"type\":\"string\",\"description\":\"Target entity ID (e.g., light.living_room). Optional if data contains entity_id.\"},"
          "\"data\":{\"type\":\"object\",\"description\":\"Additional service data as JSON object. Keys are merged into the request payload (e.g., brightness, color_name, temperature).\"},"
          "\"service_data\":{\"type\":\"string\",\"description\":\"Legacy alias for 'data' parameter — prefer using 'data' instead for new integrations. JSON string of service data.\"}"
        "},\"required\":[\"domain\",\"service\"]}",
        ha_call_service_handler);

    registry_register("ha_get_history",
        "Get the state history of a HomeAssistant entity. Requires HASS_TOKEN. "
        "Optional: start_time, end_time (ISO format).",
        "{\"type\":\"object\",\"properties\":{"
          "\"entity_id\":{\"type\":\"string\",\"description\":\"Entity ID (e.g., sensor.temperature)\"},"
          "\"start_time\":{\"type\":\"string\",\"description\":\"Start time ISO format (optional)\"},"
          "\"end_time\":{\"type\":\"string\",\"description\":\"End time ISO format (optional)\"}"
        "},\"required\":[\"entity_id\"]}",
        ha_get_history_handler);
}
