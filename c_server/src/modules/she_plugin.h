#ifndef SHE_PLUGIN_H
#define SHE_PLUGIN_H

#include <stddef.h>

/* Forward declarations */
typedef struct she_server she_server_t;
typedef struct she_connection she_connection_t;

/**
 * Handler return codes mirroring Lighttpd & SHE modular pipeline.
 */
typedef enum {
    SHE_HANDLER_GO_ON = 0,         /* Proceed to next plugin in chain */
    SHE_HANDLER_FINISHED,          /* Request was completely handled by this plugin */
    SHE_HANDLER_COMEBACK,          /* Re-evaluate pipeline */
    SHE_HANDLER_WAIT_FOR_EVENT,    /* Non-blocking event wait */
    SHE_HANDLER_ERROR              /* Internal error occurred */
} she_handler_t;

typedef struct she_plugin she_plugin_t;

struct she_plugin {
    size_t version;
    const char *name;
    const char *description;
    
    /* Lifecycle hooks */
    she_handler_t (*init)(she_server_t *srv, she_plugin_t *p);
    she_handler_t (*cleanup)(she_server_t *srv, she_plugin_t *p);
    
    /* Request processing hooks */
    she_handler_t (*handle_uri_clean)(she_server_t *srv, she_connection_t *con, void *p_data);
    she_handler_t (*handle_subrequest_start)(she_server_t *srv, she_connection_t *con, void *p_data);
    she_handler_t (*handle_response_done)(she_server_t *srv, she_connection_t *con, void *p_data);
    
    void *data;
};

#define SHE_MAX_PLUGINS 16

typedef struct {
    she_plugin_t *plugins[SHE_MAX_PLUGINS];
    size_t count;
} she_plugin_array_t;

/* Backward compatibility */
typedef she_handler_t handler_t;
typedef she_plugin_t plugin_t;
typedef she_plugin_array_t plugin_array_t;

#define HANDLER_GO_ON          SHE_HANDLER_GO_ON
#define HANDLER_FINISHED       SHE_HANDLER_FINISHED
#define HANDLER_COMEBACK       SHE_HANDLER_COMEBACK
#define HANDLER_WAIT_FOR_EVENT SHE_HANDLER_WAIT_FOR_EVENT
#define HANDLER_ERROR          SHE_HANDLER_ERROR
#define MAX_PLUGINS            SHE_MAX_PLUGINS

/* Plugin manager functions */
void she_plugins_init(she_server_t *srv);
void she_plugins_cleanup(she_server_t *srv);
int she_plugins_register(she_server_t *srv, she_plugin_t *p);

she_handler_t she_plugins_call_handle_uri_clean(she_server_t *srv, she_connection_t *con);
she_handler_t she_plugins_call_handle_subrequest_start(she_server_t *srv, she_connection_t *con);
she_handler_t she_plugins_call_handle_response_done(she_server_t *srv, she_connection_t *con);

/* Compatibility macros */
#define plugins_init she_plugins_init
#define plugins_cleanup she_plugins_cleanup
#define plugins_register she_plugins_register
#define plugins_call_handle_uri_clean she_plugins_call_handle_uri_clean
#define plugins_call_handle_subrequest_start she_plugins_call_handle_subrequest_start
#define plugins_call_handle_response_done she_plugins_call_handle_response_done

#endif /* SHE_PLUGIN_H */
