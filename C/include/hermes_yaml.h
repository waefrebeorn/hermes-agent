/**
 * @defgroup hermes_yaml YAML Parser
 * @brief YAML configuration parser.
 *
 *
Thin wrapper over libyaml for reading hermes config files.
Supports key-value pairs, nested objects, arrays, and
environment variable expansion.
 *
 * @{
 */
#ifndef HERMES_YAML_H
#define HERMES_YAML_H

/*
 * hermes_yaml.h — Compatibility shim: redirects to libyaml.
 * The old API and new libyaml API are IDENTICAL (same function names, types).
 * This file exists only so consumers don't need to change their includes.
 */

#include "../lib/libyaml/yaml.h"

/** @} */ /* end of hermes_yaml group */
#endif /* HERMES_YAML_H */
