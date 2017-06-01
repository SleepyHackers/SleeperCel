#define _GNU_SOURCE
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
/*
#define O_WRONLY 00000001
#define O_CREAT  00000100
#define O_TRUNC  00001000
#define O_DSYNC  00010000
*/

#define assignfunc(_fp, _sym)							\
  if ((!(_fp)) && (!(_fp = dlsym(RTLD_NEXT, _sym)))) {	\
    fprintf(logfp, ">>DLSYM: %s [fatal]\n", dlerror());	\
    return 3; }

#define chkwrap(real)								   \
  if (!real) {										   \
    if (resolve_syms()) {							   \
	  fprintf(logfp, ">>RESOLVE_SYMS: failed\n");	   \
	  exit(3);									   \
	}  }											   \

//Types
typedef int     (*openfunc)   (const char* pathname, int flags, ...);
typedef int     (*openatfunc) (int dirfd, const char* pathname, ...);
typedef ssize_t (*writefunc)  (int fd, const void *buf, size_t count);
typedef ssize_t (*readfunc)   (int fd, void* buf, size_t count);
typedef off_t   (*lseekfunc)  (int fd, off_t offset, int whence);
typedef int     (*closefunc)  (int fd);
typedef int     (*fcntlfunc)  (int fd, int cmd, ...);
typedef int     (*ioctlfunc)  (int fd, unsigned long req, ...);
typedef int     (*mainfunc)   (int argc, char* argv[]);

static uint8_t static_ioctls[][36] = {
  {0x00, 0x45, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, //Default TCGETS response for stdout from GNU screen terminal
   0xbf, 0x00, 0x00, 0x00, 0x3b, 0x8a, 0x00, 0x00,
   0x00, 0x03, 0x1c, 0x7f, 0x15, 0x04, 0x00, 0x01,
   0x00, 0x11, 0x13, 0x1a, 0x00, 0x12, 0x0f, 0x17,
   0x16, 0x00, 0x00, 0x00},
  {0x00, 0x55, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, //Above response modified to enable IXOFF+IXON
   0xbf, 0x00, 0x00, 0x00, 0x3b, 0x8a, 0x00, 0x00,
   0x00, 0x03, 0x1c, 0x7f, 0x15, 0x04, 0x00, 0x01,
   0x00, 0x11, 0x13, 0x1a, 0x00, 0x12, 0x0f, 0x17,
   0x16, 0x00, 0x00, 0x00}
};

//Global variables
//Pointers to real libc funcs
openfunc    realopen;
openatfunc  realopenat;
writefunc   realwrite;
readfunc    realread;
lseekfunc   reallseek;
closefunc   realclose;
fcntlfunc   realfcntl;
ioctlfunc   realioctl;
mainfunc    realmain;
//Bytes written/read to disk
uint64_t wbytes = 0;
uint64_t rbytes = 0;
//Logfile
FILE* logfp = NULL;
int32_t logfd;

/*Resolve all needed symbols and set the signal handler Hopefully this will run before programs main() is called,
  but that depends on if we're linked with the right C library. chkwrap() should ensure pointers are resolved on
  demnad if necessary.*/
__attribute__ ((constructor))
int resolve_syms() {
  assignfunc(realopen, "open");
  assignfunc(realopenat, "openat");
  
  if ((!(logfd)) && ((logfd = realopen("/tmp/sandbox.so.log", O_CREAT|O_TRUNC|O_WRONLY|O_DSYNC, S_IRWXU|S_IRGRP|S_IROTH)) == -1))
	{ exit(1); }
  if ((!(logfp)) && (!(logfp = fdopen(logfd, "w"))))
	{ exit(2); }
  fprintf(logfp, ">>RESOLVE_SYMS: logging to '/tmp/sandbox.so.log' @ [%d]\n", logfd);
  
  assignfunc(realwrite, "write");
  assignfunc(realread, "read");
  assignfunc(reallseek, "lseek");
  assignfunc(realclose, "close");
  assignfunc(realfcntl, "fcntl");
  assignfunc(realioctl, "ioctl");

  return 0;
}
//Output statistics at end of program
__attribute__ ((destructor))
void output_counts() {
  fprintf(logfp, ">>Disk I/O: %d bytes written, %d bytes read\n", wbytes, rbytes);
}

//Function wrappers
int open(const char *pathname, int flags, ...) {
  chkwrap(realopen);
  va_list ap;
  va_start(ap, flags);
  int fd = realopen(pathname, flags, ap);
  va_end(ap);
  fprintf(logfp, ">>OPEN:    pathname '%s', flags %d: fd %d\n", pathname, flags, fd);
  return fd;
}

int openat(int dirfd, const char* pathname, int flags, ...) {
  chkwrap(realopenat);
  va_list ap;
  va_start(ap, flags);
  int fd = realopenat(dirfd, pathname, flags, ap);
  va_end(ap);
  fprintf(logfp, ">>OPENAT:%12s %d, pathname '%s', flags %d %27s %d\n", "dirfd", dirfd, pathname, flags, "fd", fd);
  return fd;
}

ssize_t write(int fd, const void* buf, size_t count) {
  chkwrap(realwrite);
  ssize_t bytes = realwrite(fd, buf, count);
  fprintf(logfp, ">>WRITE:   fd %d, count %d: %d bytes written\n", fd, count, bytes);
  wbytes += bytes;
  return bytes;
}

ssize_t read(int fd, void* buf, size_t count) {
  chkwrap(realread);
  ssize_t bytes = realread(fd, buf, count);
  fprintf(logfp, ">>READ:%11s %d, count %d %60s %d\n", "fd", fd, count, "bytes read", bytes);
  rbytes += bytes;
  return bytes;
}

off_t lseek(int fd, off_t offset, int whence) {
  chkwrap(reallseek);
  int ret = reallseek(fd, offset, whence);
  fprintf(logfp, ">>SEEK:    fd %d, offset %d, whence %d: return value %d\n", fd, offset, whence, ret);
  return ret;
}

int close(int fd) {
  chkwrap(realclose);
  int ret = realclose(fd);
  fprintf(logfp, ">>CLOSE:%10s %d %75s %d\n", "fd", fd, "return value", ret);
  return ret;
}

int fcntl(int fd, int cmd, ...) {
  chkwrap(realfcntl);
  va_list ap;
  int ret;
  if (cmd == 3) {
	switch (fd) {
	case 0:
	case 1:
	case 2: return 0x8002;
	default: break;
	}
  }
  va_start(ap,cmd);
  ret = realfcntl(fd, cmd, ap);
  va_end(ap);
  fprintf(logfp, ">>FCNTL:   fd %d, cmd %d : return value %d\n", fd, cmd, ret);
  return ret;
}

int ioctl(int fd, unsigned long req, ...) {
  chkwrap(realioctl);
  va_list ap;
  int ret;
  void* p;
  
  if (req == TCGETS) {
	switch (fd) {
	case 0:
	case 1:
	case 2: //Inject static response from static_ioctl and return successfully
	  va_start(ap, req);
	  p = va_arg(ap, void*);
	  memcpy(p, (void*)(static_ioctls[1]), 36); //IXON/IXOFF enabled
	  va_end(ap);
	  fprintf(logfp, ">>IOCTL: fd %d, req %d : ** injected static response **\n", fd, req);
	  return 0;
	default: break;
	}
  }

  //Conditions for injection weren't met, proceed normally
  va_start(ap, req);
  ret = realioctl(fd, req, ap);
  va_end(ap);
  fprintf(logfp, ">>IOCTL: fd %d, req %d : return value %X\n", fd, req, ret);
  return ret;
}

//void _start() {}

//int clone...
