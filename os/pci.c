#include "types.h"
#include "serial.h"

#define DRVID_I217 1

#define MAX_PCI 128
/* move to types.h */
typedef uint8_t bit;
/* *************** */

/* move to pci.h */
typedef struct {
  bit zero : 2;
  bit reg  : 6;
  bit func : 3;
  bit dev  : 5;
  bit bus  : 8;
  bit res  : 7;
  bit enable : 1;
} pci_config;

typedef struct {
  uint8_t used;
  uint8_t bus;
  uint8_t slot;
  uint8_t func;
  uint16_t vendor;
  uint16_t device;
  uint8_t type;
  uint16_t bar[6];
  uint32_t* erom;
} pci_device;

typedef struct {
  uint16_t vendor;
  uint16_t device;
} pci_record;

typedef struct {
  uint32_t id;
  char name[64];
  pci_record devices[128];
} known_pci;
/* ************* */

known_pci known_pci_table[] = {
  {DRVID_I217, "Intel i217 Gigabit Ethernet", {
      {0x8086, 0x1049}, {0x8086, 0x104a}, {0x8086, 0x104b}, {0x8086, 0x104c},
      {0x8086, 0x104d}, {0x8086, 0x105e}, {0x8086, 0x105f}, {0x8086, 0x1060},
      {0x8086, 0x107d}, {0x8086, 0x107e}, {0x8086, 0x107f}, {0x8086, 0x108b},
      {0x8086, 0x108c}, {0x8086, 0x1096}, {0x8086, 0x1098}, {0x8086, 0x109a},
      {0x8086, 0x10a4}, {0x8086, 0x10a5}, {0x8086, 0x10b9}, {0x8086, 0x10ba},
      {0x8086, 0x10bb}, {0x8086, 0x10bc}, {0x8086, 0x10bd}, {0x8086, 0x10bf},
      {0x8086, 0x10c0}, {0x8086, 0x10c2}, {0x8086, 0x10c3}, {0x8086, 0x10c4},
      {0x8086, 0x10c5}, {0x8086, 0x10cb}, {0x8086, 0x10cc}, {0x8086, 0x10cd},
      {0x8086, 0x10ce}, {0x8086, 0x10d3}, {0x8086, 0x10d5}, {0x8086, 0x10d9},
      {0x8086, 0x10da}, {0x8086, 0x10de}, {0x8086, 0x10df}, {0x8086, 0x10e5},
      {0x8086, 0x10ea}, {0x8086, 0x10eb}, {0x8086, 0x10ef}, {0x8086, 0x10f0},
      {0x8086, 0x10f5}, {0x8086, 0x10f6}, {0x8086, 0x1501}, {0x8086, 0x1502},
      {0x8086, 0x1503}, {0x8086, 0x150c}, {0x8086, 0x1525}, {0x8086, 0x153a},
      {0x8086, 0x153b}, {0x8086, 0x1559}, {0x8086, 0x155a}, {0x8086, 0x15a0},
      {0x8086, 0x15a1}, {0x8086, 0x15a2}, {0x8086, 0x15a3}, {0x8086, 0x294c},
      {0},
    }
  },
  {0}
};

pci_device pci_table[MAX_PCI];

extern uint32_t inportl(uint16_t p_port);
extern void outportl(uint16_t p_port, uint32_t p_data);
extern void memcpy(void* dst, void* src, size_t sz);
extern void memset(void* dst, char c, size_t sz);
extern size_t strnlen(char* s, size_t n);

#define xchg(_a, _b) _a ^= _b; _b ^= _a; _a ^= _b

uint32_t swap32(uint32_t i) {
  uint16_t* p = (uint16_t*)&i;
  xchg(p[0], p[1]);
  return i;
}

uint16_t swap16(uint16_t i) {
  uint8_t* p = (uint8_t*)&i;
  xchg(p[0], p[1]);
  return i;
}

uint16_t pci_config_readw(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
  uint32_t addr;
  uint32_t lbus = (uint32_t)bus;
  uint32_t lslot = (uint32_t)slot;
  uint32_t lfunc = (uint32_t)func;
  uint16_t tmp = 0;
  
  addr = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) |
                    (off & 0xfc) | ((uint32_t)0x80000000));
  outportl(0xCF8, addr);
  tmp = (uint16_t)((inportl(0xCFC) >> ((off & 2) * 8)) & 0xffff);
  return tmp;
}

uint8_t pci_config_readb(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
  uint32_t addr;
  uint32_t lbus = (uint32_t)bus;
  uint32_t lslot = (uint32_t)slot;
  uint32_t lfunc = (uint32_t)func;
  uint8_t tmp = 0;
  
  addr = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) |
                    (off & 0xfc) | ((uint32_t)0x80000000));
  outportl(0xCF8, addr);
  tmp = (uint8_t)((inportl(0xCFC) >> ((off & 2) * 8)) & 0xff);
  return tmp;
}

uint32_t pci_config_readl(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
  uint32_t addr;
  uint32_t lbus = (uint32_t)bus;
  uint32_t lslot = (uint32_t)slot;
  uint32_t lfunc = (uint32_t)func;
  uint32_t tmp = 0;
  
  addr = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) |
                    (off & 0xfc) | ((uint32_t)0x80000000));
  outportl(0xCF8, addr);
  tmp = (uint32_t)((inportl(0xCFC) >> ((off & 2) * 8)) & 0xffffffff);
  return tmp;
}

uint32_t* pci_config_readp(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
  uint32_t addr;
  uint32_t lbus = (uint32_t)bus;
  uint32_t lslot = (uint32_t)slot;
  uint32_t lfunc = (uint32_t)func;
  uint32_t* tmp = 0;
  
  addr = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) |
                    (off & 0xfc) | ((uint32_t)0x80000000));
  outportl(0xCF8, addr);
  tmp = (uint32_t*)((inportl(0xCFC) >> ((off & 2) * 8)) & 0xffffffff);
  return tmp;
}

typedef struct {
  bit zero : 1;
  bit type : 2;
  bit prefetch : 1;
  uint16_t addr;
} pci_memory_bar;

typedef struct {
  bit one: 1;
  bit res: 1;
  bit addr: 4;
} pci_io_bar;

uint8_t pci_probe_device(pci_device* dev, uint8_t bus, uint8_t slot, uint8_t func) {
  pci_memory_bar* mbar;
  pci_io_bar* iobar;
  uint32_t bar = 0, off = 16, tmp;
  if ((dev->vendor = swap16(pci_config_readw(bus, slot, func, 0))) != 0xffff) {
    dev->func = func;
    dev->bus = bus; dev->slot = slot;
    dev->used = 1;
    dev->device = swap16(pci_config_readw(bus, slot, func, 2));
    //    dev->type = (uint8_t)pci_config_readw(bus, slot, func, 14));
    dev->type = pci_config_readb(bus, slot, func, 14);
    switch(dev->type) {
    case 0x80:
      //multifunction device
      //break;
    case 0x00:
      while (bar < 6) {
        tmp = pci_config_readl(bus, slot, func, off);
        if (!(tmp & 1)) {
          mbar = (pci_memory_bar*)(&tmp);
          ioprintf(COM1, "[debug] (pci) pci_probe_device: MEMORY bar: %w\r\n",
                   mbar->addr);
          //dev->bar[bar] = swap16((uint16_t)(pci_config_readl(bus, slot, func, off) >> 4u));
          dev->bar[bar] = mbar->addr; //swap16((tmp >> 4u) & 0xFFF0); 
        }
        else {
          iobar = (pci_io_bar*)(&tmp);
          ioprintf(COM1, "[debug] (pci) pci_probe_device: I/O bar: %b\r\n",
                   iobar->addr);
          //dev->bar[bar] = swap16((uint16_t)(pci_config_readl(bus, slot, func, off) >> 2u));
          dev->bar[bar] = iobar->addr; //swap16((tmp >> 2u) & 0xFFF0);
        }
        bar++;
        off += 4;
      /*dev->bar[0] = swap16((uint16_t)(pci_config_readl(bus, slot, func, 16) >> 4u));
      dev->bar[1] = swap16((uint16_t)(pci_config_readl(bus, slot, func, 20) >> 4u));
      dev->bar[2] = swap16((uint16_t)(pci_config_readl(bus, slot, func, 24) >> 4u));
      dev->bar[3] = swap16((uint16_t)(pci_config_readl(bus, slot, func, 28) >> 4u));
      dev->bar[4] = swap16((uint16_t)(pci_config_readl(bus, slot, func, 32) >> 4u));
      dev->bar[5] = swap16((uint16_t)(pci_config_readl(bus, slot, func, 36) >> 4u));*/
        dev->erom   = swap16((uint16_t)(pci_config_readl(bus, slot, func, 52) >> 4u));
      }
      break;
    case 0x01:
      ioprintf(COM1, "[warn] (pci) pci_probe_device: PCI-to-PCI bridge not supported\r\n");
      break;
    case 0x02:
      ioprintf(COM1, "[warn] (pci) pci_probe_device: CardBus bridge not supported\r\n");
      break;
    default:
      ioprintf(COM1, "[warn] (pci) pci_probe_device: Device with unknown type '%x'\r\n",
               dev->type);
      break;
    }
    return 0;
  }
  dev->vendor = 0;
  dev->used = 0;
  return 1;
}

uint8_t pci_early_probe(uint8_t bus, uint8_t slot) {
  if ((swap16(pci_config_readw(bus, slot, 0, 0))) != 0xffff) {
    return 0;
  }
  return 1;
}

void pci_probe() {
  uint8_t bus, slot, func, c = 0;
  ioprintf(COM1, "Probing PCI ...\n");
  for (bus = 0; bus < 255; bus++) {
    for (slot = 0; slot < 31; slot++) {
      if (!(pci_early_probe(bus, slot))) {
        for (func = 0; func < 7; func++) {
          if (!(pci_probe_device(&pci_table[c], bus, slot, func))) {
            ioprintf(COM1, "<bus: %d> <slot: %d> <func: %d>\r\n", bus, slot, func);
            c++;
            if (c >= MAX_PCI-1) { return; }
          }
        }
      }
    }
  }
}

known_pci* pci_lookup(uint16_t vendor, uint16_t device) {
  known_pci* a = known_pci_table;
  pci_record* b;
  while (a->id) {
    b = a->devices;
    while (b->vendor) {
      if (b->vendor == swap16(vendor) &&
          b->device == swap16(device)) {
          return a;
        }
        b = &b[1];
    }
    a = &a[1];
  }
  return NULL;
}

void pci_print() {
  char* name;
  uint32_t i;
  known_pci* k;
  ioprint(COM1, "--- PCI table ---\r\n");
  for (i = 0; i < MAX_PCI; i++ ) {
    if (pci_table[i].used) {
      if ((k = pci_lookup(pci_table[i].vendor, pci_table[i].device))) {
        name = k->name;
      }
      else {
        name = "Unrecognized device";
      }
      ioprintf(COM1,
               "<%s>\r\n"
               "* Vendor: 0x%w\r\n"
               "* Device: 0x%w\r\n"
               "* Type: 0x%b\r\n"
               "* Bus: 0x%b\r\n"
               "* Slot: 0x%b\r\n"
               "* Function: 0x%b\r\n"
               "* BAR0: 0x%w\r\n"
               "* BAR1: 0x%w\r\n"
               "* BAR2: 0x%w\r\n"
               "* BAR3: 0x%w\r\n"
               "* BAR4: 0x%w\r\n"
               "* BAR5: 0x%w\r\n"
               "* EROM: 0x%w\r\n",
               name, pci_table[i].vendor, pci_table[i].device,
               pci_table[i].type,
               pci_table[i].bus, pci_table[i].slot, pci_table[i].func,
               pci_table[i].bar[0], pci_table[i].bar[1], pci_table[i].bar[2],
               pci_table[i].bar[3], pci_table[i].bar[4], pci_table[i].bar[5],
               pci_table[i].erom);
    }
  }
}

void init_pci() {
  memset(pci_table, 0, sizeof(pci_device) * MAX_PCI);
  pci_probe();
  pci_print();
}
