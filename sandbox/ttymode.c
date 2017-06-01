#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

/*#define b2fmt "%c%c%c%c%c%c%c%c"
#define base2(byte)			 \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \	
  (byte & 0x04 ? '1' : '0'), \					
  (byte & 0x02 ? '1' : '0'), \					
  (byte & 0x01 ? '1' : '0') */


static char buf[17] = {0};
static uint16_t mode = 0;
static uint8_t fd = 0;

__attribute__ ((always_inline))
static inline char* i10_o2(uint16_t x, char buf[static 17]) {
    char *s = buf + 17;
    *--s = 0;
    if (!x)
	  { *--s = '0'; }
    for (; x; x /= 2)
	  { *--s = '0' + x % 2; }
    return s;
}

int main() {
  for (; fd<3; fd++) {
	if ((mode = fcntl(fd, F_GETFL)) == 0x8000) {
	  write(1, "fcntl err\n", 11);
	  return errno;
	}
	if (((write(1, i10_o2(mode, buf), 16) == -1) || ((write(1, "\n", 1)) == -1))) {
	  write(1, "write err\n", 11);
	  return errno;
	}   
  }
}
