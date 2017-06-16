#ifndef _CONFIG_H
#define _CONFIG_H

#include <libconfig.h>

typedef struct main_config {
  int ssl_enabled;
  int websocket_port;
  char *ssl_cert_file;
  char *ssl_key_file;
} MAIN_CONFIG;

#endif

