#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>

/*If IXOFF is set start/stop input control is enabled. The system shall transmit one or more STOP characters which are intended to cause the terminal device to stop transmitting data as needed to prevent the input queue from overflowing and causing loss of data and shall transmit one or more START characters which shall cause the terminal device to resume transmitting data, as soon as the device can continue transmitting data without risk of overflowing the input queue. - Set on termios.c_iflag*/
#define IXOFF 0010000
/*If IXON is set start/stop output control is enabled. A received STOP character shall suspend output and a received START character shall restart output. When IXON is set START and STOP characters are not read but merely perform flow-control functions. When IXON is not set the START and STOP characters are read.*/
#define IXON  0002000
#define NCCS 19 //Number of control characters? [http://lxr.free-electrons.com/source/include/uapi/asm-generic/termbits.h#L17]

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
static int32_t fd = 0;

int main(int argc, char* argv[]) {
  if (argc < 2) {
	write(1, "Syntax: ioctl_out <filename>\n", 30);
	return 1;
  }
  
  if (ioctl(1, TCGETS, &ioctl_static) == -1) {
	return errno;
  }

  ioctl_static.c_iflag |= IXOFF;
  ioctl_static.c_iflag |= IXON;
  
  if ((fd = open(argv[1], O_RDWR|O_CREAT|O_TRUNC|O_DSYNC, S_IRWXU|S_IRGRP|S_IROTH)) == -1) {
	write(1, "open failed\n", 13);
	return errno;
  } 

  if ((write(fd, &ioctl_static, sizeof(struct termios))) == -1) {
	write(1, "write failed\n", 14);
	return errno;
  }
  
  /*  if (fsync(fd) == -1) {
	write(1, "fsync failed\n", 14);
	return errno;
	}*/

  /*  if (close(fd) == -1) {
	write(1, "close failed\n", 14);
	return errno;
	}*/
}

