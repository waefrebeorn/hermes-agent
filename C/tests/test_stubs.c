/* test_stubs.c — minimal stubs for linking Hermes tool tests */
#include <stdlib.h>
#include <stdio.h>

/* hermes_error.c stubs */
void hermes_error(int code, const char *fmt, ...) { (void)code; (void)fmt; }
void hermes_vlog(int level, const char *tag, const char *fmt, ...) { (void)level; (void)tag; (void)fmt; }

/* secrets stub */
const char *get_api_key(const char *provider) { (void)provider; return NULL; }
void get_slermes_home(char *buf, size_t sz) { (void)buf; (void)sz; }

/* tool_config stub */
const char *tool_config_get(const char *section, const char *key) { (void)section; (void)key; return NULL; }

/* plugin stubs */
int plugin_load(const char *path) { (void)path; return 0; }
void *plugin_sym(void *handle, const char *name) { (void)handle; (void)name; return NULL; }

/* db stubs */
int db_open(const char *path) { (void)path; return -1; }

/* registry stub */
int registry_register(const char *name, const char *desc, const char *schema, void *handler) {
    (void)name; (void)desc; (void)schema; (void)handler; return 0;
}
