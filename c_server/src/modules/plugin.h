#ifndef PLUGIN_H
#define PLUGIN_H

#include <stddef.h>

/* Forward declarations */
typedef struct server server_t;
typedef struct connection connection_t;

/**
 * Handler return codes mirroring Lighttpd's handler_t.
 */
typedef enum {
    HANDLER_GO_ON = 0,         /* Proceed to the next plugin in the chain */
    HANDLER_FINISHED,          /* Request was completely handled by this plugin */
    HANDLER_COMEBACK,          /* Re-evaluate pipeline */
    HANDLER_WAIT_FOR_EVENT,    /* Non-blocking wait */
    HANDLER_ERROR              /* Error occurred */
} handler_t;

typedef struct plugin plugin_t;

struct plugin {
    size_t version;
    const char *name;
    const char *description;
    
    /* Lifecycle hooks */
    handler_t (*init)(server_t *srv, plugin_t *p);
    handler_t (*cleanup)(server_t *srv, plugin_t *p);
    
    /* Request processing hooks */
    handler_t (*handle_uri_clean)(server_t *srv, connection_t *con, void *p_data);
    handler_t (*handle_subrequest_start)(server_t *srv, connection_t *con, void *p_data);
    handler_t (*handle_response_done)(server_t *srv, connection_t *con, void *p_data);
    
    void *data;
};

#define MAX_PLUGINS 16

typedef struct {
    plugin_t *plugins[MAX_PLUGINS];
    size_t count;
} plugin_array_t;

/* Plugin manager functions */
void plugins_init(server_t *srv);
void plugins_cleanup(server_t *srv);
int plugins_register(server_t *srv, plugin_t *p);

handler_t plugins_call_handle_uri_clean(server_t *srv, connection_t *con);
handler_t plugins_call_handle_subrequest_start(server_t *srv, connection_t *con);
handler_t plugins_call_handle_response_done(server_t *srv, connection_t *con);

#endif /* PLUGIN_H */
