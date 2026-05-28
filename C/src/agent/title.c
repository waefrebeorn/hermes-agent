/*
 * title.c — Session title generation for Hermes C.
 * Simple extractive summarization: uses first N words of first message.
 */

#include "hermes_agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char *agent_generate_title(llm_config_t *cfg, const char *first_message) {
    (void)cfg;

    if (!first_message || !*first_message)
        return strdup("New Session");

    char buf[256];
    size_t pos = 0;
    bool in_code_block = false;

    buf[0] = '\0';

    /* Extract first meaningful sentence — skip code blocks */
    for (const char *p = first_message; *p && pos < sizeof(buf) - 3; p++) {
        /* Track code block boundaries */
        if (*p == '`' && *(p+1) == '`' && *(p+2) == '`') {
            in_code_block = !in_code_block;
            p += 2;
            continue;
        }
        if (in_code_block) continue;

        /* Skip leading whitespace/newlines */
        if (pos == 0 && (*p == ' ' || *p == '\n' || *p == '\r')) continue;

        unsigned char c = (unsigned char)*p;
        if (c == '\n') {
            /* Convert newline to space, stop on double newline */
            if (*(p+1) == '\n') break;
            if (pos > 0 && buf[pos-1] != ' ') buf[pos++] = ' ';
        } else if (c == ' ') {
            /* Collapse consecutive spaces */
            if (pos > 0 && buf[pos-1] != ' ') buf[pos++] = ' ';
        } else if (isprint(c)) {
            buf[pos++] = (char)c;
            /* Stop at sentence-ending punctuation followed by space then EOS/newline */
            if ((c == '.' || c == '!' || c == '?') && pos < sizeof(buf) - 2) {
                const char *next = p + 1;
                while (*next == ' ' || *next == '\n') next++;
                if (*next == '\0' || *next == '\n') break;
            }
        }
    }

    buf[pos] = '\0';

    /* Trim trailing space */
    while (pos > 0 && buf[pos-1] == ' ') buf[--pos] = '\0';

    /* Cap at 80 chars */
    if (pos > 80) {
        buf[80] = '\0';
        pos = 80;
        while (pos > 0 && buf[pos] != ' ') pos--;
        if (pos > 0) buf[pos] = '\0';
    }

    /* Trim trailing period */
    while (pos > 0 && buf[pos-1] == '.') buf[--pos] = '\0';

    if (pos == 0) return strdup("New Session");
    return strdup(buf);
}
