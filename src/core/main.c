#include <stdlib.h>
#include "sleepercel.h"
#include "ws.h"
#include "config.h"

int main(int argc, char *argv[], char **envp)
{
  int ret_val;
  MAIN_CONFIG our_config;
  
  init_config(&our_config);
  if(ret_val != EXIT_SUCCESS) goto exit_sleeper;
  
  //  init_websockets();
  
  //  world_loop();

 exit_sleeper:
  destroy_config(&our_config);
  return ret_val;
}
