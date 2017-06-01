#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define NCCS 19 //Number of control characters? [http://lxr.free-electrons.com/source/include/uapi/asm-generic/termbits.h#L17]

#define MSG_TCGETS_FAIL 0

typedef unsigned char cc_t;
typedef unsigned int  speed_t;
typedef unsigned int  tcflag_t;

struct termios {
  tcflag_t c_iflag;
  tcflag_t c_oflag;
  tcflag_t c_cflag;
  tcflag_t c_lflag;
  cc_t c_line;
  cc_t c_cc[NCCS];
};

static struct termios ioctl_static = {0};
static struct termios* t = &ioctl_static;
static char* messages[] = {
  "ioctl TCGETS failed\n"
};

int main() {
  if (ioctl(1, TCGETS, t) == -1) {
	write(2, messages[MSG_TCGETS_FAIL], strlen(messages[MSG_TCGETS_FAIL]));
	return errno;
  }
}
