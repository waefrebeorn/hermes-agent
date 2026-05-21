#ifndef HERMES_YAML_H
#define HERMES_YAML_H

/*
 * slermes_yaml.h — Compatibility shim: redirects to libyaml.
 * The old API and new libyaml API are IDENTICAL (same function names, types).
 * This file exists only so consumers don't need to change their includes.
 */

#include "../lib/libyaml/yaml.h"

#endif /* HERMES_YAML_H */
