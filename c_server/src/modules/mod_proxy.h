#ifndef MOD_PROXY_H
#define MOD_PROXY_H

#include "modules/she_plugin.h"

/**
 * mod_proxy: SHE Reverse Proxy and Upstream Gateway Subsystem
 *
 * Implements reverse proxy pass-through with X-Forwarded-* headers,
 * round-robin upstream load balancing, and upstream health status.
 */
she_plugin_t *mod_proxy_plugin_init(void);

#endif /* MOD_PROXY_H */
