/* Stub implementations for gateway platform init functions.
 * Used by test_gateway.c to resolve symbols without linking all platforms. */

#include "hermes.h"

void yuanbao_start(void) {}
void weixin_start(void) {}
void msgraph_webhook_run(void) {}
void msgraph_webhook_init(void) {}
void email_set_from(const char *s) { (void)s; }
void platform_feishu_init(void) {}
void feishu_send_message(const char *c, const char *t) { (void)c; (void)t; }
int email_send_message_ext(const char *t, const char *s, const char *b, const char *f, const char *h) {
    (void)t; (void)s; (void)b; (void)f; (void)h; return 0;
}
