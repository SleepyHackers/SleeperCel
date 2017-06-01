#pragma once

#define COM1 0x3F8
#define ioprint(_p, _s) _ioprint(_p, _s, strnlen(_s, 4095)+1)
#define ioprintf(_p, _s, ...) ionprintf(_p, strnlen(_s, 4095), _s, ##__VA_ARGS__)

void init_comport(uint16_t port);
char serial_rx(uint16_t port);
void serial_tx(uint16_t port, char c);
uint8_t serial_rx_ready(uint16_t port);
void _ioprint(uint16_t port, char* s, uint32_t sz);
void ionprintf(uint16_t port, uint32_t sz, char* s, ...);
