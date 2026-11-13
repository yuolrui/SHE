#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "modules/mod_cloud_mesh.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include "core/she_chunkqueue.h"
#include "core/she_buffer.h"

static she_cloud_mesh_state_t g_cloud_mesh;
static int g_cloud_mesh_initialized = 0;

static void she_cloud_mesh_init_defaults(void) {
    if (g_cloud_mesh_initialized) return;

    memset(&g_cloud_mesh, 0, sizeof(g_cloud_mesh));
    snprintf(g_cloud_mesh.cluster_id, sizeof(g_cloud_mesh.cluster_id), "she-cloud-mesh-prod");
    snprintf(g_cloud_mesh.container_image, sizeof(g_cloud_mesh.container_image), "she-engine:1.0-distroless");
    snprintf(g_cloud_mesh.pod_namespace, sizeof(g_cloud_mesh.pod_namespace), "ingress-system");
    g_cloud_mesh.liveness_ok = 1;
    g_cloud_mesh.readiness_ok = 1;
    g_cloud_mesh.probes_served = 0;
    g_cloud_mesh_initialized = 1;
}

static she_handler_t she_cloud_mesh_init(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    she_cloud_mesh_init_defaults();
    return SHE_HANDLER_GO_ON;
}

static she_handler_t she_cloud_mesh_cleanup(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    return SHE_HANDLER_GO_ON;
}

static she_handler_t she_cloud_mesh_handle_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)p_data;
    if (!con || !con->request.uri_path) return SHE_HANDLER_GO_ON;

    she_cloud_mesh_init_defaults();
    const char *path = she_buffer_cstr(con->request.uri_path);
    if (!path) return SHE_HANDLER_GO_ON;

    if (strcmp(path, "/cloud-mesh/healthz") == 0 ||
        strcmp(path, "/cloud-mesh/readyz") == 0 ||
        strcmp(path, "/cloud-mesh-status") == 0) {
        g_cloud_mesh.probes_served++;
        int ready = (srv && srv->is_running && g_cloud_mesh.readiness_ok) ? 1 : 0;

        char body[1024];
        int body_len = snprintf(body, sizeof(body),
            "{\n"
            "  \"module\": \"mod_cloud_mesh\",\n"
            "  \"probe\": \"%s\",\n"
            "  \"status\": \"%s\",\n"
            "  \"cluster_id\": \"%s\",\n"
            "  \"container_image\": \"%s\",\n"
            "  \"namespace\": \"%s\",\n"
            "  \"plugins_ready\": %zu,\n"
            "  \"active_connections\": %u,\n"
            "  \"probes_served\": %" PRIu64 "\n"
            "}\n",
            path,
            ready ? "READY" : "DRAINING",
            g_cloud_mesh.cluster_id,
            g_cloud_mesh.container_image,
            g_cloud_mesh.pod_namespace,
            srv ? srv->plugins.count : (size_t)0,
            srv ? srv->stats.active_connections : 0U,
            g_cloud_mesh.probes_served
        );

        con->http_status = ready ? 200 : 503;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        she_buffer_append_string(con->response_headers, ready ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 503 Service Unavailable\r\n");
        she_buffer_append_string(con->response_headers, "Server: SHE/1.0 (Secure HTTP Engine - Cloud Mesh Sidecar)\r\n");
        she_buffer_append_string(con->response_headers, "Content-Type: application/json; charset=utf-8\r\n");
        she_buffer_append_string(con->response_headers, "X-SHE-Cloud-Mesh: Distroless-Sidecar\r\n");
        char clen[64];
        snprintf(clen, sizeof(clen), "Content-Length: %d\r\n\r\n", body_len);
        she_buffer_append_string(con->response_headers, clen);

        con->state = SHE_CON_STATE_RESPONSE_START;
        she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
        she_chunkqueue_append_mem(con->write_queue, body, (size_t)body_len);
        return SHE_HANDLER_FINISHED;
    }

    return SHE_HANDLER_GO_ON;
}

she_plugin_t *mod_cloud_mesh_plugin_init(void) {
    she_plugin_t *p = (she_plugin_t *)calloc(1, sizeof(she_plugin_t));
    if (!p) return NULL;

    p->version = 1;
    p->name = "mod_cloud_mesh";
    p->description = "Containerized Cloud Mesh Sidecar & Kubernetes Healthz/Readyz Probe Controller";
    p->init = she_cloud_mesh_init;
    p->cleanup = she_cloud_mesh_cleanup;
    p->handle_subrequest_start = she_cloud_mesh_handle_subrequest_start;

    return p;
}
