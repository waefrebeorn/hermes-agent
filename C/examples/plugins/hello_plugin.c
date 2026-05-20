/*
 * hello_plugin.c — Minimal Hermes C plugin example.
 * Compile: gcc -shared -fPIC -o hello_plugin.so hello_plugin.c
 * Demonstrates the basic plugin interface convention.
 */

#include <stdio.h>
#include <string.h>

/* Required: plugin metadata */
const char *plugin_meta_name(void) {
    return "hello-plugin";
}

const char *plugin_meta_version(void) {
    return "1.0.0";
}

const char *plugin_get_type(void) {
    return "example";
}

/* Required: init / cleanup */
int plugin_init(void) {
    printf("[plugin:hello] Hello from Hermes C plugin!\n");
    printf("[plugin:hello] Version: %s\n", plugin_meta_version());
    return 0;
}

int plugin_cleanup(void) {
    printf("[plugin:hello] Goodbye from Hermes C plugin!\n");
    return 0;
}
