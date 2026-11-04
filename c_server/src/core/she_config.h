#ifndef SHE_CONFIG_H
#define SHE_CONFIG_H

#include "core/she_server.h"

int she_config_load_file(she_server_config_t *cfg, const char *filepath);
int she_config_save_file(const she_server_config_t *cfg, const char *filepath);

#endif /* SHE_CONFIG_H */
