#include <stdarg.h>
#include "types.h"
#include "serial.h"

#define COM1 0x3F8

#define rx_ready(_port) (inportb(_port + 5) & 1)
#define tx_ready(_port) (inportb(_port + 5) & 0x20)

extern void outportb(uint16_t port, uint8_t val);
extern uint8_t inportb(uint16_t port);


uint8_t serial_rx_ready(uint16_t port) {
  return rx_ready(port);
}

char serial_rx(uint16_t port) {
  while (!(rx_ready(port))) { /* spinlock until data received */ }
  return inportb(port);
}

void serial_tx(uint16_t port, char c) {
  while (!(tx_ready(port))) { /* spinlock until buffer empty */ }
  outportb(port, c);
}


//blocks
void _ioprint(uint16_t port, char* s, uint32_t sz) {
  uint32_t i;
  for (i = 0; i < sz; i++) {
    if (!s[i]) { break; }
    serial_tx(port, s[i]);
  }
}

void init_comport(uint16_t port) {
  outportb(port + 1, 0x00);
  outportb(port + 3, 0x80);
  outportb(port,     0x01);
  outportb(port + 1, 0x00);
  outportb(port + 3, 0x03);
  outportb(port + 2, 0xc7);
  outportb(port + 4, 0x0b);
  _ioprint(port, "\e[0m\e[2J", 8);
}

extern void p2s(void* _p, char* s, size_t sz);
extern void i2s(uint32_t i, char* s, size_t sz);
extern size_t strnlen(char* s, size_t n);
extern void* memset(void* s, uint8_t c, size_t n);

void ionprintf(uint16_t port, uint32_t sz, char* s, ...) {
  char* p;
  uint32_t c = 0;
  char buf[8];
  va_list ap;

  va_start(ap, s);
  while (s[0]) {
    if (s[0] == '%') {
      s++;
      memset(buf, 0, 8);
      switch(s[0]) {
      case 'b':
        i2s(va_arg(ap, unsigned int), buf, 1);
        ioprint(port, buf);
        break;
      case 'w':
        i2s(va_arg(ap, unsigned int), buf, 2);
        ioprint(port, buf);
        break;
      case 'd':
      case 'i':
        i2s(va_arg(ap, unsigned int), buf, 4);
        ioprint(port, buf);
        _ioprint(port, buf, strnlen(buf, 4)+1);
        break;
      case 'x':
        p2s(va_arg(ap, unsigned int), buf, 4);
        ioprint(port, buf);
        break;
      case 's':
        p = va_arg(ap, char*);
        _ioprint(COM1, p, strnlen(p, 4095)+1);
        break;
      default:
        serial_tx(port, '?');
        break;
      }
    }
    else {
      if (s[0]) {
        serial_tx(port, s[0]);
      }
    }
    s++;
    if ((c++) >= sz) {
      break;
    }
  }
  va_end(ap);
}

