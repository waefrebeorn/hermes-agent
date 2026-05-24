/*
 * manual_compression_feedback.c — User-facing summaries for manual compression.
 *
 * Port of Python agent/manual_compression_feedback.py (49 lines).
 * MIT License — WuBu Hermes Project
 */

#include "manual_compression_feedback.h"
#include <stdio.h>
#include <string.h>

void summarize_manual_compression(int before_count, int after_count,
                                  int before_tokens, int after_tokens,
                                  compression_feedback_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));

    bool noop = (after_count == before_count);
    out->noop = noop;

    if (noop) {
        if (before_tokens < 0) before_tokens = 0;
        if (after_tokens < 0) after_tokens = 0;

        snprintf(out->headline, sizeof(out->headline),
                 "No changes from compression: %d messages", before_count);

        if (after_tokens == before_tokens) {
            snprintf(out->token_line, sizeof(out->token_line),
                     "Approx request size: ~%d tokens (unchanged)", before_tokens);
        } else {
            snprintf(out->token_line, sizeof(out->token_line),
                     "Approx request size: ~%d \xe2\x86\x92 ~%d tokens",
                     before_tokens, after_tokens);
        }
    } else {
        snprintf(out->headline, sizeof(out->headline),
                 "Compressed: %d \xe2\x86\x92 %d messages", before_count, after_count);

        snprintf(out->token_line, sizeof(out->token_line),
                 "Approx request size: ~%d \xe2\x86\x92 ~%d tokens",
                 before_tokens, after_tokens);
    }

    /* Note: fewer messages but more tokens is counterintuitive */
    if (!noop && after_count < before_count && after_tokens > before_tokens) {
        snprintf(out->note, sizeof(out->note),
                 "Note: fewer messages can still raise this estimate when "
                 "compression rewrites the transcript into denser summaries.");
    }
}
