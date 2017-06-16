
#include "ws.h"

int main(int argc, char *argv[], char **envp)
{
  int ret_val;

  init_websockets();
  
  world_loop();

  return ret_val;
}
