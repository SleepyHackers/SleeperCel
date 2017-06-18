#include "config.h"
#include <stdlib.h>
#include <stdio.h>

int init_config(MAIN_CONFIG *config)
{
  config_t cfg;
  config_setting_t *setting;
  char *error_report = (char*)malloc(MAX_ERROR_SIZE);
  
  config_init(&cfg);

  if(!config_read_file(&cfg, MAIN_CONFIG_FILE))
  {
    sprintf(error_report, "Config ERROR: %s:%d - %s\n", config_error_file(&cfg),
            config_error_line(&cfg), config_error_text(&cfg));
    goto config_fail;
  }
  
  if(!config_lookup_int(&cfg, "websocket_port", &config->websocket_port)) {
    sprintf(error_report, "Config ERROR: websocket_port is a required option!\n");
    goto config_fail;
  }

  if(!config_lookup_int(&cfg, "ssl_enabled", &config->ssl_enabled)) {
    sprintf(error_report, "Config ERROR: ssl_enabled is a required option!\n");
    goto config_fail;
  }

  if(!config_lookup_string(&cfg, "module_autoload_file", &config->module_autoload_file)) {
    sprintf(error_report, "Config ERROR: module_autoload_file is a required option!\n");
    goto config_fail;
  }

  if(!config_lookup_string(&cfg, "module_autoload_dir", &config->module_autoload_dir)) {
    sprintf(error_report, "Config ERROR: module_autoload_dir is a required option!\n");
    goto config_fail;
  }

  /* Load SSL configs if enabled */
  if(config->ssl_enabled) {
    if(!config_lookup_string(&cfg, "ssl_cert_file", &config->ssl_cert_file)) {
      sprintf(error_report, "Config ERROR: ssl_cert_file is a required option when ssl_enabled=1!\n");
      goto config_fail;
    }
    if(!config_lookup_string(&cfg, "ssl_key_file", &config->ssl_key_file)) {
      sprintf(error_report, "Config ERROR: ssl_key_file is a required option when ssl_enabled=1!\n");
      goto config_fail;
    }
  } else {
    config->ssl_cert_file = NULL;
    config->ssl_key_file = NULL;
  }
  //  config_lookup_string(&cfg, "name", &config->)

  //printf("port: %d", config->websocket_port);

  config_destroy(&cfg);
  free(error_report);
  return(EXIT_SUCCESS);

 config_fail:
  config_destroy(&cfg);
  fprintf(stderr, "%s", error_report);
  free(error_report);
  return(EXIT_FAILURE);

}

void destroy_config(MAIN_CONFIG *config) {
}
