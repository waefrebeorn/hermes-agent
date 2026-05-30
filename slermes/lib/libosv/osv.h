#ifndef HERMES_OSV_H
#define HERMES_OSV_H

/*
 * osv.h — OSV malware check for MCP extension packages.
 * Port of Python tools/osv_check.py.
 *
 * Before launching an MCP server via npx/uvx, queries the OSV API to
 * check if the package has known malware advisories (MAL-* IDs).
 * Regular CVEs are ignored — only confirmed malware is blocked.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check if an MCP server package has known malware advisories.
 *
 * Inspects the command (e.g. "npx", "uvx") and args to infer the
 * package name and ecosystem. Queries the OSV API for MAL-* advisories.
 *
 * @param command  The command name (e.g. "npx", "uvx").
 * @param args     Array of argument strings.
 * @param arg_count Number of arguments in args.
 * @return  An error message string if malware is found (caller must free),
 *          or NULL if clean/unknown/error.
 */
char *osv_check_package_for_malware(const char *command,
                                     const char **args,
                                     int arg_count);

/**
 * Infer package ecosystem from the command name.
 * @param command  The command name string.
 * @return  "npm", "PyPI", or NULL if unrecognized (static string, no free).
 */
const char *osv_infer_ecosystem(const char *command);

/**
 * Extract package name and optional version from command args.
 * @param args       Array of argument strings.
 * @param arg_count  Number of arguments.
 * @param ecosystem  The ecosystem string ("npm" or "PyPI").
 * @param out_name   [out] Package name (caller must free).
 * @param out_version [out] Version string or NULL (caller must free).
 * @return 0 on success, -1 if not parseable.
 */
int osv_parse_package(const char **args, int arg_count,
                      const char *ecosystem,
                      char **out_name, char **out_version);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_OSV_H */
