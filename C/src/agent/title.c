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
    (void)cfg; /* Title gen could use LLM in future */

    if (!first_message || !*first_message)
        return strdup("New Session");

    /* Extract first 6 words as title */
    char buf[256];
    size_t pos = 0;
    int words = 0;
    bool in_word = false;

    for (const char *p = first_message; *p && pos < sizeof(buf) - 3; p++) {
        unsigned char c = (unsigned char)*p;
        if (isspace(c) || c == '\n' || c == '\r') {
            if (in_word) {
                in_word = false;
                if (words < 6) buf[pos++] = ' ';
            }
        } else if (isprint(c)) {
            if (!in_word) {
                in_word = true;
                words++;
                if (words > 6) break; /* Stop after reading all of word 6 */
            }
            buf[pos++] = (char)c;
        }
    }
    buf[pos] = '\0';

    /* Trim trailing space */
    while (pos > 0 && buf[pos-1] == ' ') buf[--pos] = '\0';

    if (pos == 0) return strdup("New Session");
    return strdup(buf);
}
