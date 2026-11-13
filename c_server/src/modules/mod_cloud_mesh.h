#ifndef MOD_CLOUD_MESH_H
#define MOD_CLOUD_MESH_H

#include <stddef.h>
#include <stdint.h>
#include "modules/she_plugin.h"

typedef struct {
    char cluster_id[32];         /* "she-cloud-mesh-prod" */
    char container_image[48];    /* "she-engine:1.0-distroless" */
    char pod_namespace[24];      /* "ingress-system" */
    int liveness_ok;
    int readiness_ok;
    uint64_t probes_served;
} she_cloud_mesh_state_t;

/* Plugin initialization */
she_plugin_t *mod_cloud_mesh_plugin_init(void);

#endif /* MOD_CLOUD_MESH_H */
