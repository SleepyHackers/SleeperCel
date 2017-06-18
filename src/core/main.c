#include <stdlib.h>
#include "sleepercel.h"
#include "ws.h"
#include "config.h"

#define RUN_INIT(func) \
  ret_val = func(&our_config); if(ret_val != EXIT_SUCCESS) goto exit_sleeper;

int main(int argc, char *argv[], char **envp)
{
  int ret_val;
  MAIN_CONFIG our_config;

  RUN_INIT(init_config);
  //  RUN_INIT(init_websockets);
  
  //  world_loop();

 exit_sleeper:
  destroy_config(&our_config);
  return ret_val;
}
