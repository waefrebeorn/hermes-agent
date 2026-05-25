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
        snprintf(buf, sz, "Authorization: Bearer %s\r\n", token);
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

/* Make HA GET request */
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

/* ha_list_entities: List all entities (GET /api/states) */
char *ha_list_entities_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;
    return ha_get_json("/states");
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

/* ha_list_services: List all services (GET /api/services) */
char *ha_list_services_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;
    return ha_get_json("/services");
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
    const char *service_data_str = json_get_str(args, "service_data", "{}");

    if (!domain || !*domain || !service || !*service) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"Missing 'domain' or 'service' parameter\"}");
    }

    http_t *h = http_new(15);
    if (!h) { json_free(args); return strdup("{\"success\":false,\"error\":\"OOM\"}"); }

    char auth[256];
    ha_auth_hdr(auth, sizeof(auth));

    char url[1024];
    snprintf(url, sizeof(url), "%s/api/services/%s/%s", ha_base(), domain, service);

    http_resp_t *r = http_post_json_auth(h, url, service_data_str, auth);

    char *json = NULL;
    if (r && r->body) {
        size_t sz = strlen(r->body) + 128;
        json = (char *)malloc(sz);
        if (json) {
            if (r->status == 200)
                snprintf(json, sz, "{\"success\":true,\"data\":%s}", r->body);
            else
                snprintf(json, sz, "{\"success\":false,\"error\":\"HA returned HTTP %d\"}", r->status);
        }
    } else {
        json = strdup("{\"success\":false,\"error\":\"HTTP request failed\"}");
    }

    if (r) http_resp_free(r);
    http_free(h);
    json_free(args);
    return json ? json : strdup("{\"success\":false,\"error\":\"OOM\"}");
}

/* ================================================================
 *  Registration
 * ================================================================ */

void registry_init_homeassistant(void) {
    registry_register("ha_list_entities",
        "List all HomeAssistant entities and their current state. Requires HASS_TOKEN.",
        "{\"type\":\"object\",\"properties\":{}}",
        ha_list_entities_handler);

    registry_register("ha_get_state",
        "Get the state of a specific HomeAssistant entity. Requires HASS_TOKEN.",
        "{\"type\":\"object\",\"properties\":{\"entity_id\":{\"type\":\"string\",\"description\":\"Entity ID (e.g., light.living_room)\"}},\"required\":[\"entity_id\"]}",
        ha_get_state_handler);

    registry_register("ha_list_services",
        "List all available HomeAssistant services. Requires HASS_TOKEN.",
        "{\"type\":\"object\",\"properties\":{}}",
        ha_list_services_handler);

    registry_register("ha_call_service",
        "Call a HomeAssistant service. Requires HASS_TOKEN.",
        "{\"type\":\"object\",\"properties\":{\"domain\":{\"type\":\"string\",\"description\":\"Service domain (e.g., light, switch)\"},\"service\":{\"type\":\"string\",\"description\":\"Service name (e.g., turn_on, turn_off)\"},\"service_data\":{\"type\":\"string\",\"description\":\"Optional JSON service data\"}},\"required\":[\"domain\",\"service\"]}",
        ha_call_service_handler);
}
