/* Centralized Nous Portal request tags.
 *
 * Python equivalent: agent/portal_tags.py
 *
 * Every Hermes request that hits the Nous Portal — main agent loop, auxiliary
 * client, and any future code path — must carry the same product-attribution
 * tags so Nous can attribute usage to Hermes Agent and bucket it by release.
 */

#include "hermes_portal_tags.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Version string — must match include/hermes.h */
#ifndef HERMES_VERSION
#define HERMES_VERSION "0.15.1-slermes"
#endif

char *hermes_client_tag(void)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "client=hermes-client-v%s", HERMES_VERSION);
    return strdup(buf);
}

char *hermes_nous_portal_tags_json(void)
{
    char buf[256];
    char *client_tag = hermes_client_tag();
    if (!client_tag)
        return NULL;

    snprintf(buf, sizeof(buf),
        "[\"product=hermes-agent\",\"%s\"]", client_tag);
    free(client_tag);
    return strdup(buf);
}
