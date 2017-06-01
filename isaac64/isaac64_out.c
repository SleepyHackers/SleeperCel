#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include "isaac64/isaac64.h"

static uint64_t buf[512]; //4K blocks
static uint64_t c = 0;
static uint16_t i;

//Expects a 256-byte seed via stdin
static int8_t init_isaac() {
  static uint8_t i;
  static uint32_t* randp = (uint32_t*)randrsl;
  if (read(0, randrsl, 256) == -1) {
	return errno;
  }
  randinit(1);
}

//Outputs ISAAC entropy to stdout in 4k synchronous blocks
int main(int argc, char* restrict argv[]) {  
  //  memset(buf, 0, 4096);
  for (;;) {
	if (!(c % 1048576)) { // seed every 256 4k-sized blocks
	  if (init_isaac()) {
		return -1;
	  }
	}
	for (i=0; i<512; i++) { buf[i] = isaac_rand(); }
	if (write(1, buf, 4096) == -1) { 
	  return errno;
	}
	c += 4096;
  }
}
