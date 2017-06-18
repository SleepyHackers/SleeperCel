#ifndef _CONFIG_H
#define _CONFIG_H

#include <libconfig.h>
#include "sleepercel.h"

typedef struct main_config {
  int ssl_enabled;
  int websocket_port;
  const char *ssl_cert_file;
  const char *ssl_key_file;
  const char *module_autoload_file;
  const char *module_autoload_dir;
} MAIN_CONFIG;

extern int init_config(MAIN_CONFIG *config);
extern void destroy_config(MAIN_CONFIG *config);

#endif

