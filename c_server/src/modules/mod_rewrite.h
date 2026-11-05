#ifndef MOD_REWRITE_H
#define MOD_REWRITE_H

#include "modules/she_plugin.h"

/**
 * mod_rewrite: SHE URL Rewriting and HTTP 301/302 Redirection Subsystem
 *
 * Implements prefix and pattern matching for:
 * 1. HTTP 301 (Permanent) and 302 (Temporary) redirects with Location header.
 * 2. Internal transparent URI rewriting before docroot resolution.
 */
she_plugin_t *mod_rewrite_plugin_init(void);

#endif /* MOD_REWRITE_H */
