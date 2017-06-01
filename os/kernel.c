#include <stdatomic.h>
#include <stdarg.h>
#include "types.h"
#include "rand.h"
#include "serial.h"

#define KRAM_SZ 65536
#define KRAM_BLOCKS 1024

#define TASK_STATE_SZ 4096
#define SCHED_TASKS_MAX 16
#define TASK_RETURN 0
#define TASK_ERR 1
#define TASK_YIELD 2
#define TASK_CONT 3
#define TASK_STOP 4

typedef struct {
  uint32_t flags;
  uint32_t mem_lower;
  uint32_t mem_upper;
  uint32_t boot_device;
  char* cmdline;
} multiboot_info;
typedef struct block block;
struct block {
  void* addr; //If addr is non-null, block is used
  uint32_t sz;
  block* next;
  block* prev;
  uint8_t used;
};
typedef struct {
  uint32_t pid;
  uint32_t quantum;
  char name[16];
  intfp run;
  uint8_t state[TASK_STATE_SZ];
  uint8_t active;
} task_data;
typedef struct {
  char base10;
  char* base16;
  uint8_t len;
} p2s_record;


//static rng seed
uint32_t seed[256] =
  {0x113d, 0x6c54, 0x311d, 0x7122, 0x79fc, 0x36d9, 0x6ec6, 0x1637, 0x7871, 0x642a, 0x1234, 0x337d, 0x6fc0, 0x3d19, 0x72b2, 0x3bef, 0x705f, 0x70e2, 0x2139,
   0x5a7b, 0x6019, 0x2b4a, 0x315b, 0x5c33, 0x4663, 0x22c3, 0x36dc, 0x6417, 0x3481, 0xaaf,  0x78d9, 0x1fda, 0x4869, 0x205a, 0x1d0c, 0x2de4, 0x778d, 0x511b,
   0x4d0c, 0x5b94, 0x65b0, 0x457d, 0x30c3, 0x6500, 0x6f0c, 0x1215, 0x5dbe, 0x1d50, 0x755,  0x181f, 0x1dd8, 0x7e08, 0x4f96, 0xe59,  0x1e38, 0x7da0, 0x2826,
   0xf4f,  0x1b64, 0x4ed6, 0x75d3, 0x7fcf, 0x192c, 0x56a8, 0x2622, 0x4640, 0x64ab, 0x5016, 0x3b23, 0x331a, 0x4d10, 0x183,  0x1864, 0xd6f,  0x2deb, 0x7c9a,
   0x101e, 0x4a6a, 0x2015, 0x72f0, 0x3213, 0x4c89, 0x74e,  0x374c, 0x2374, 0x5cbf, 0x3bd0, 0xda4,  0x5f3f, 0x6917, 0x57d,  0x11a1, 0xa92,  0x22b7, 0x74ed,
   0x27e6, 0x266c, 0x2eab, 0x130e, 0x2bf2, 0x631e, 0x72d4, 0x139d, 0x5b44, 0x75c1, 0x7d00, 0x44c8, 0x793c, 0x2cac, 0x16c,  0x2726, 0x5476, 0x3437, 0x5dd0,
   0x4065, 0x4600, 0x5f13, 0x1eb4, 0x18ab, 0x48fc, 0x5f45, 0x5e99, 0x419c, 0x285b, 0x3443, 0x77fc, 0x1838, 0x60bc, 0x7c94, 0x7eed, 0x4ba7, 0x78a1, 0x4e22,
   0x6ab0, 0x2166, 0x5c93, 0x623,  0x167,  0x6439, 0x2941, 0x236c, 0x33be, 0x39c5, 0x6ac7, 0x6e8d, 0x4280, 0x2894, 0x30b4, 0x3ba9, 0x2d11, 0x4b,   0x7afe,
   0x7f97, 0x4672, 0x41cf, 0x1e6d, 0x4704, 0x2a2d, 0x48eb, 0x931,  0x3348, 0x5979, 0x4241, 0x5a39, 0x966,  0x4a2f, 0x7cb6, 0x6a9f, 0x392d, 0xd4e,  0x3bb2,
   0x4ce5, 0x10e9, 0x71bb, 0x522d, 0x5a1b, 0x4974, 0x87b,  0x629f, 0x7260, 0x58d3, 0x4595, 0x6cce, 0x6f71, 0x3c64, 0xd0b,  0x1e9e, 0x359b, 0x570,  0x3183,
   0x5877, 0x1ae1, 0x52f4, 0x6893, 0x4e00, 0x3737, 0x2728, 0x10f2, 0x5196, 0x120f, 0x735f, 0x2684, 0x4908, 0x69ac, 0x501a, 0x1d09, 0x224,  0x6718, 0x2170,
   0x7812, 0x2097, 0x73c4, 0x6f52, 0x434d, 0x347e, 0x65f5, 0x144b, 0x172a, 0x7a71, 0x4724, 0x56e0, 0x47f6, 0x239d, 0x3d93, 0x4c7b, 0x6cb5, 0x3201, 0x7fee,
   0x2648, 0x137f, 0x3a28, 0x4466, 0x4922, 0x1ccb, 0x2f24, 0x130,  0x48dc, 0x8bf,  0x63f1, 0x22ea, 0x5657, 0x34c0, 0x4f96, 0x25f8, 0x6f5e, 0x2295, 0x4207,
   0x7ed7, 0x1758, 0x56c5, 0x4f61, 0x2e7d, 0x66dc, 0x44e2, 0x15c2, 0x2aff};
p2s_record p2s_table[] = {
  {-127, "0", 1},  {-126, "1", 1},  {-125, "2", 1},  {-124, "3", 1},  {-123, "4", 1},  {-122, "5", 1},  {-121, "6", 1},  {-120, "7", 1}, 
  {-119, "8", 1},  {-118, "9", 1},  {-117, "a", 1},  {-116, "b", 1},  {-115, "c", 1},  {-114, "d", 1},  {-113, "e", 1},  {-112, "f", 1}, 
  {-111, "10", 2},  {-110, "11", 2},  {-109, "12", 2},  {-108, "13", 2},  {-107, "14", 2},  {-106, "15", 2},  {-105, "16", 2},  {-104, "17", 2}, 
  {-103, "18", 2},  {-102, "19", 2},  {-101, "1a", 2},  {-100, "1b", 2},  {-99, "1c", 2},  {-98, "1d", 2},  {-97, "1e", 2},  {-96, "1f", 2}, 
  {-95, "20", 2},  {-94, "21", 2},  {-93, "22", 2},  {-92, "23", 2},  {-91, "24", 2},  {-90, "25", 2},  {-89, "26", 2},  {-88, "27", 2}, 
  {-87, "28", 2},  {-86, "29", 2},  {-85, "2a", 2},  {-84, "2b", 2},  {-83, "2c", 2},  {-82, "2d", 2},  {-81, "2e", 2},  {-80, "2f", 2}, 
  {-79, "30", 2},  {-78, "31", 2},  {-77, "32", 2},  {-76, "33", 2},  {-75, "34", 2},  {-74, "35", 2},  {-73, "36", 2},  {-72, "37", 2}, 
  {-71, "38", 2},  {-70, "39", 2},  {-69, "3a", 2},  {-68, "3b", 2},  {-67, "3c", 2},  {-66, "3d", 2},  {-65, "3e", 2},  {-64, "3f", 2}, 
  {-63, "40", 2},  {-62, "41", 2},  {-61, "42", 2},  {-60, "43", 2},  {-59, "44", 2},  {-58, "45", 2},  {-57, "46", 2},  {-56, "47", 2}, 
  {-55, "48", 2},  {-54, "49", 2},  {-53, "4a", 2},  {-52, "4b", 2},  {-51, "4c", 2},  {-50, "4d", 2},  {-49, "4e", 2},  {-48, "4f", 2}, 
  {-47, "50", 2},  {-46, "51", 2},  {-45, "52", 2},  {-44, "53", 2},  {-43, "54", 2},  {-42, "55", 2},  {-41, "56", 2},  {-40, "57", 2}, 
  {-39, "58", 2},  {-38, "59", 2},  {-37, "5a", 2},  {-36, "5b", 2},  {-35, "5c", 2},  {-34, "5d", 2},  {-33, "5e", 2},  {-32, "5f", 2}, 
  {-31, "60", 2},  {-30, "61", 2},  {-29, "62", 2},  {-28, "63", 2},  {-27, "64", 2},  {-26, "65", 2},  {-25, "66", 2},  {-24, "67", 2}, 
  {-23, "68", 2},  {-22, "69", 2},  {-21, "6a", 2},  {-20, "6b", 2},  {-19, "6c", 2},  {-18, "6d", 2},  {-17, "6e", 2},  {-16, "6f", 2}, 
  {-15, "70", 2},  {-14, "71", 2},  {-13, "72", 2},  {-12, "73", 2},  {-11, "74", 2},  {-10, "75", 2},  {-9, "76", 2},  {-8, "77", 2}, 
  {-7, "78", 2},  {-6, "79", 2},  {-5, "7a", 2},  {-4, "7b", 2},  {-3, "7c", 2},  {-2, "7d", 2},  {-1, "7e", 2},  {0, "7f", 2}, 
  {1, "80", 2},  {2, "81", 2},  {3, "82", 2},  {4, "83", 2},  {5, "84", 2},  {6, "85", 2},  {7, "86", 2},  {8, "87", 2}, 
  {9, "88", 2},  {10, "89", 2},  {11, "8a", 2},  {12, "8b", 2},  {13, "8c", 2},  {14, "8d", 2},  {15, "8e", 2},  {16, "8f", 2}, 
  {17, "90", 2},  {18, "91", 2},  {19, "92", 2},  {20, "93", 2},  {21, "94", 2},  {22, "95", 2},  {23, "96", 2},  {24, "97", 2}, 
  {25, "98", 2},  {26, "99", 2},  {27, "9a", 2},  {28, "9b", 2},  {29, "9c", 2},  {30, "9d", 2},  {31, "9e", 2},  {32, "9f", 2}, 
  {33, "a0", 2},  {34, "a1", 2},  {35, "a2", 2},  {36, "a3", 2},  {37, "a4", 2},  {38, "a5", 2},  {39, "a6", 2},  {40, "a7", 2}, 
  {41, "a8", 2},  {42, "a9", 2},  {43, "aa", 2},  {44, "ab", 2},  {45, "ac", 2},  {46, "ad", 2},  {47, "ae", 2},  {48, "af", 2}, 
  {49, "b0", 2},  {50, "b1", 2},  {51, "b2", 2},  {52, "b3", 2},  {53, "b4", 2},  {54, "b5", 2},  {55, "b6", 2},  {56, "b7", 2}, 
  {57, "b8", 2},  {58, "b9", 2},  {59, "ba", 2},  {60, "bb", 2},  {61, "bc", 2},  {62, "bd", 2},  {63, "be", 2},  {64, "bf", 2}, 
  {65, "c0", 2},  {66, "c1", 2},  {67, "c2", 2},  {68, "c3", 2},  {69, "c4", 2},  {70, "c5", 2},  {71, "c6", 2},  {72, "c7", 2}, 
  {73, "c8", 2},  {74, "c9", 2},  {75, "ca", 2},  {76, "cb", 2},  {77, "cc", 2},  {78, "cd", 2},  {79, "ce", 2},  {80, "cf", 2}, 
  {81, "d0", 2},  {82, "d1", 2},  {83, "d2", 2},  {84, "d3", 2},  {85, "d4", 2},  {86, "d5", 2},  {87, "d6", 2},  {88, "d7", 2}, 
  {89, "d8", 2},  {90, "d9", 2},  {91, "da", 2},  {92, "db", 2},  {93, "dc", 2},  {94, "dd", 2},  {95, "de", 2},  {96, "df", 2}, 
  {97, "e0", 2},  {98, "e1", 2},  {99, "e2", 2},  {100, "e3", 2},  {101, "e4", 2},  {102, "e5", 2},  {103, "e6", 2},  {104, "e7", 2}, 
  {105, "e8", 2},  {106, "e9", 2},  {107, "ea", 2},  {108, "eb", 2},  {109, "ec", 2},  {110, "ed", 2},  {111, "ee", 2},  {112, "ef", 2}, 
  {113, "f0", 2},  {114, "f1", 2},  {115, "f2", 2},  {116, "f3", 2},  {117, "f4", 2},  {118, "f5", 2},  {119, "f6", 2},  {120, "f7", 2}, 
  {121, "f8", 2},  {122, "f9", 2},  {123, "fa", 2},  {124, "fb", 2},  {125, "fc", 2},  {126, "fd", 2},  {127, "fe", 2}
};

uint8_t kram[KRAM_SZ];
block kram_blocks[KRAM_BLOCKS];

randctx ctx = {0};
randctx* isaac_ctx = &ctx;

uint32_t inportl(uint16_t p_port) {
  uint32_t l_ret;
  asm volatile ("inl %1, %0" : "=a" (l_ret) : "dN" (p_port));
  return l_ret;
}

void outportl (uint16_t p_port, uint32_t p_data) {
  asm volatile ("outl %1, %0" : : "dN" (p_port), "a" (p_data));
}

void outportb(uint16_t port, uint8_t val) { asm("outb %b0,%w1" : : "a"(val), "d"(port)); }
uint8_t inportb(uint16_t port) {
  uint8_t r;
  asm volatile("inb %1, %0" : "=a"(r) : "Nd"(port));
  return r;
}

void* memcpy(void* dst, void* src, size_t sz) {
  uint32_t i;
  for (i=0; i<sz; i++) { ((uint8_t*)dst)[i] = ((uint8_t*)src)[i]; }
  return dst;
}

void* memcpy2(void* dst, void* src, size_t sz) {
  uint32_t i;
  uint32_t c = 0;
  for (i=0; i<sz/4; i++) {
    ((uint32_t*)dst)[i] = ((uint32_t*)src)[i];
    c += 4;
  }
  if (c < sz) {
    for (i=c; i<sz; i++) { ((uint8_t*)dst)[i] = ((uint8_t*)src)[i]; }
  }
  return dst;
}

void* memset(void* s, uint8_t c, size_t n) {
  size_t i;
  for (i=0; i<n; i++) { ((uint8_t*)s)[i] = c; }
  return s;
}

void* memsetw(uint16_t* s, uint16_t c, size_t n) {
  size_t i;
  for (i=0; i<n; i++) { ((uint16_t*)s)[i] = c; }
  return s;
}

size_t strnlen(char* s, size_t n) {
  uint32_t i;
  for (i=0; i<n; i++) { if (!(s[i])) { break; } }
  return i-1;
}

static void init_isaac(randctx* ctx) {
  memset(ctx, 0, sizeof(randctx));
  memcpy(ctx->randrsl, seed, 256*4);
  randinit(ctx, TRUE);
}

static void* kmalloc(mint sz) {
  block* f;
  block* p = &kram_blocks[0];
  mint i = 0;
  while (p) {
    if ((!p->used) && (sz <= p->sz)) {
      break;
    }
    p=p->next;
  }
  if (!p) {
    return NULL;
  }
  for (i=0; i<KRAM_BLOCKS; i++) {
    if (!(kram_blocks[i].addr)) {
        break;
    }
  }
  if (i >= KRAM_BLOCKS) {
    return NULL;
  }
  f = &(kram_blocks[i]);
  f->addr = p->addr+sz+1;
  f->sz = p->sz - sz;
  p->sz = sz;
  p->used = 1;
  p->next = f;
  f->prev = p;

  return p->addr;
}

static void kfree(void* addr) {
  block* p = &kram_blocks[0];
  block* old;
  while (p) {
    if ((p->used) && (p->addr == addr)) { break; }
    p=p->next;
  }
  if (!p) { //Nop if address couldn't be found in block list
    return;
  }

  if ((p->next) && (!p->next->used)) { //Merge right
    p->sz += p->next->sz;
    old = p->next;
    p->next = p->next->next;
    if (p->next) { p->next->prev = p; }
    memset(old, 0, sizeof(block));
  }
  if ((p->prev) && (!p->prev->used)) { //Shift left, then merge right
    p = p->prev;
    p->sz += p->next->sz;
    old = p->next;
    p->next = p->next->next;
    if (p->next) { p->next->prev = p; }
    memset(old, 0, sizeof(block));
  }
  p->used = 0;
  memset(p->addr, 0, p->sz);   
}

void p2s(void* _p, char* s, size_t sz) {
  char* p = (char*)&_p;
  
  size_t c = 0;
  p2s_record* r; // = &p2s_table[0];
  
  if (sz < 4) { return; }
  
  s[0] = '0';
  s[1] = 'x';

  s += 2;
  
  while (c < sz) {
    r = &p2s_table[(unsigned char)p[c]];
    memcpy(s, r->base16, r->len + 1);
    s += r->len + 1;
    c++;
  }
  *s = 0;
}

//this doesnt really do what we want yet as it outputs in hex
void i2s(uint32_t i, char* s, size_t sz) {
  char* p = (char*)&i;

  size_t c = 0;
  p2s_record* r;

  if (!sz) { return; }

  while (c < sz) {
    r = &p2s_table[(unsigned char)p[c]];
    memcpy(s, r->base16, r->len + 1);
    s += r->len;
    c++;
  }
  s[0] = 0;
}

//////////////////////////////////////////////////////////////
//@@@@@@@@@@@@@@@@@@@ Global functions @@@@@@@@@@@@@@@@@@@@@//
//////////////////////////////////////////////////////////////

int strncmp(char* a, char* b, unsigned int max) {
  uint32_t i = 0;
  uint32_t ret = 0;
  while (i < max && a[i] && b[i]) {
    if (a[i] != b[i]) { ret++; }
    i++;
  }
  return ret;
}

char response_buf[4096];
char command_buf[80];
uint8_t command_idx = 0;
uint8_t command_ready = 0;
uint8_t response_ready = 0;
j
static void cmd_syn() {
  //vtprint(vt, "[debug] test_hello: invoked\n");
  memcpy(response_buf, "ACK", 4);
  response_ready = 1;
}

static void cmd_rand() {
  //vtprint(vt, "[debug] cmd_rand: invoked\n");
  char buf[8];
  uint32_t r = rand(isaac_ctx);
  i2s(r, buf, 4);
  memcpy(response_buf, buf, strnlen(buf, 8));
  response_ready = 1;
}

static void cmd_pciprobe() {
}

static void eval_command() {
  command_idx = 0;
  if (!strncmp(command_buf, "syn", 3)) { cmd_syn(); }
  else if (!strncmp(command_buf, "rand", 4)) { cmd_rand(); }
  memset(command_buf, 0, 80);
}

//store character in command buffer
static void stcomchr(char c) {
  if (command_idx >= 78 || c == '\r' || c == '\n') {
    command_buf[command_idx] = 0;
    command_ready = 1;
    return;
  }

  command_buf[command_idx] = c;
  command_idx++;
}

int task_d(task_data* task) {
  char c;
  if (!serial_rx_ready(COM1)) {
    return TASK_YIELD;
  }
  c = serial_rx(COM1);
  stcomchr(c);
  //serial_tx(COM1, c);
  return TASK_YIELD;
}

int task_e(task_data* task) {
  //Something triggered a response that hasn't been sent out yet
  //  if (response_ready) { return TASK_YIELD; }
  //Command buffer is not yet ready to be evaluated
  if (!command_ready) { return TASK_YIELD; }
  command_ready = 0;
  //serial_tx(COM1, '\r');
  //serial_tx(COM1, '\n');
  eval_command();
  return TASK_YIELD;
}

int task_f(task_data* task) {
  //No response to transmit
  if (!response_ready) { return TASK_YIELD; }
  response_ready = 0;
  ioprintf(COM1, response_buf);
  serial_tx(COM1, '\r');
  serial_tx(COM1, '\n');
  memset(response_buf, 0, strnlen(response_buf, 4096));
  return TASK_YIELD;
}

static void scheduler() {
  int ret;
  size_t i;
  task_data tasks[SCHED_TASKS_MAX];
  memset(tasks, 0, sizeof(task_data)*SCHED_TASKS_MAX);
  
  tasks[3].pid = 4;
  memcpy(tasks[3].name, "D", 2);
  tasks[3].run = task_d;
  tasks[3].active = 1;

  tasks[4].pid = 5;
  memcpy(tasks[4].name, "E", 2);
  tasks[4].run = task_e;
  tasks[4].active = 1;

  tasks[5].pid = 6;
  memcpy(tasks[5].name, "F", 2);
  tasks[5].run = task_f;
  tasks[5].active = 1;
  
  while (1) {
    for (i = 0; i < SCHED_TASKS_MAX; i++) {
      if (tasks[i].active) {
        ret = tasks[i].run(&tasks[i]);
        switch(ret) {
        case TASK_YIELD: break;
        case TASK_RETURN:
          /*          vtprintf(vt0, "[debug] (kernel) scheduler: PID %d returned success\n",
                      tasks[i].pid);*/
          memset(&tasks[i], 0, sizeof(task_data));
          break;
        case TASK_STOP:
          /*vtprintf(vt0, "[debug] (kernel) scheduler: PID %d stopped\n", tasks[i].pid);*/
          tasks[i].active = 0;
          break;
        case TASK_ERR:
          /*vtprintf(vt0, "[error] (kernel) scheduler: PID %d exited with error status\n",
            tasks[i].pid);*/
          memset(&tasks[i], 0, sizeof(task_data));
          break;
        }
        ret = 0;
      }
    }
  }
}

extern void init_serial();
extern void init_pci();

/*static void protected() {
  asm("cli\n"
  "lgdt [gdtr]\n"
  "mov %%eax, %%cr0\n"
  "or %%eax, 1\n"
  "mov %%cr0, %%eax\n" : : : "eax");
  }*/

void kernel_main() {
  //if (!(vt0 = malloc(sizeof(vterm)))) { return; }
  //vterm_set(vt0, 80, 25, 0, 0, VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK), VGA_BASIC_START);
  
  init_comport(COM1);
  ioprint(COM1, "\r\n");
  ioprint(COM1, "COM1 enabled\r\n");
  
  ioprint(COM1, "Initializing dynamic heap\r\n");
  memset(kram, 0, KRAM_SZ);
  memset(kram_blocks, 0, sizeof(block) * KRAM_BLOCKS);
  kram_blocks[0].addr = kram;
  kram_blocks[0].sz = KRAM_SZ;
  
  ioprint(COM1, "Initializing ISAAC CSPRNG\r\n");
  init_isaac(isaac_ctx);

  ioprint(COM1, "Initializing PCI\r\n");
  init_pci();

  ioprint(COM1, "Starting kernel services\r\n");
  scheduler();
}

///////////////////////////////////// UNUSED code below for developer reference /////////////////////////////////////////////////
//VGA colors
/*#define VGA_BLACK  0
#define VGA_BLUE  1
#define VGA_GREEN  2
#define VGA_CYAN  3
#define VGA_RED  4
#define VGA_MAGENTA  5
#define VGA_BROWN  6
#define VGA_LIGHT_GREY  7
#define VGA_DARK_GREY  8
#define VGA_LIGHT_BLUE  9
#define VGA_LIGHT_GREEN  10
#define VGA_LIGHT_CYAN  11
#define VGA_LIGHT_RED  12
#define VGA_LIGHT_MAGENTA  13
#define VGA_LIGHT_BROWN  14
#define VGA_WHITE  15*/

/*#define VTIDX(_p, _a, _b) ((_b * _p->x_max) + _a)
#define VGA_COLOR(_fg, _bg) ((_bg << 4) | (_fg & 0x0F))
#define VGA_PUT(_vt, _ch, _co, _x, _y)  _vt->buf[_y * _vt->y_max + _x] = VGA_ENTRY(_ch, _co);
#define VGA_ENTRY(_uc, _co) ((uint16_t)_uc | (uint16_t)_co << 8)
#define VTPRINT(_vt, _s) _vtprint(_vt, _s, strnlen(_s, VTERM_MAX_STR)+1)

#define VGA_BASIC_START ((uint16_t*)0xB8000)
#define VTERM_MAX_STR 4096*/


/*typedef struct {
  size_t x_max; //max rows
  size_t y_max; //max cols
  size_t x; //row position
  size_t y; //column position
  size_t z; //unused for now, maybe use later for kernel supported terminal multiplexing?
  uint8_t color; //VGA colors
  uint16_t* buf; //buffer
  } vterm;*/

/*static void vterm_set(vterm* vt, size_t x_max, size_t y_max, size_t x, size_t y, uint8_t color, uint16_t* buf) {
  vt->x_max = x_max; vt->y_max = y_max;
  vt->x = x;         vt->y = y;
  vt->color = color; vt->buf = buf;
}

static void vterm_update_csr(vterm* vt) {
  outportb(0x3D4, 14); outportb(0x3D5, VTIDX(vt, vt->x, vt->y) >> 8);    
  outportb(0x3D4, 15); outportb(0x3D5, VTIDX(vt, vt->x, vt->y));
}

static void vterm_scroll(vterm* vt) {
  if (vt->y >= vt->x_max) {
    memcpy(vt->buf,
           vt->buf + ( (vt->y - 24) * (80) ),
           (((25) - (vt->y - 24)) * (80) * (2)));
    memsetw((vt->buf + (25 - (vt->y - 24)) * 80),
            0x20 | vt->color,
            80);
    vt->y = 24;
  }
  }*/

/*void vterm_put(vterm* vt, char c) {
  if      ( (c == 0x08) && (vt->x) ) { vt->x--; }
  else if (c == 0x09)                { vt->x = (vt->x + 4) & ~(4 - 1); }
  else if (c == '\r')                { vt->x = 0; }
  else if (c == '\n')                { vt->x = 0; vt->y++; }
  else if (c >= ' ') {
    vt->buf[(vt->y * vt->x_max + vt->x)] = VGA_ENTRY(c, vt->color);
    vt->x++;
  }
  if (vt->x >= vt->x_max) {
    vt->x = 0;
    vt->y++;
  }
  vterm_scroll(vt);
  vterm_update_csr(vt);
}

void vterm_clear(vterm* vt) {
  memsetw(vt->buf, (0x20 | vt->color), 25*80);
  vt->x = 0;
  vt->y = 0;
  vterm_update_csr(vt);
}

void _vtprint(vterm* vt, const char* s, size_t sz) {
  size_t i;
  for (i=0; i<sz; i++) {
    if (s[i]) { vterm_put(vt, s[i]); }
    else      { return; }
  }
}

void vtprint(vterm* vt, char* s) { VTPRINT(vt, s); }

void vtprintf(vterm* vt, char* s, ...) {
  char buf[8];
  va_list ap;
  
  va_start(ap, s);
  while (s[0]) {
    if (s[0] == '%') {
      s++;
      switch(s[0]) {
      case 'd':
      case 'i':
        memset(buf, 0, 8);
        i2s(va_arg(ap, unsigned int), buf, 4);
        vtprint(vt, buf);
        break;
      case 'x':
        memset(buf, 0, 8);
        p2s(va_arg(ap, unsigned int), buf, 4);
        vtprint(vt, buf);
        break;
      case 's':
        vtprint(vt, va_arg(ap, char*));
        break;
      default:
        vterm_put(vt, '?');
        break;
      }
    }
    else {
      vterm_put(vt, s[0]);
    }
    s++;
  }
  va_end(ap);
  }*/


  /*  memset(kram, 0, KERNEL_MEMORY_SZ);
  memset(kram_blocks, 0, KERNEL_MEMORY_BLK_MAX*sizeof(block));
  kram_blocks[0].addr = kram;
  kram_blocks[0].sz = KERNEL_MEMORY_SZ;*/
//Can be used to signify END in an array of pointers when NULL is considered valid
//#define END ((void*)0xFFFFFFFFFFFF)

//////////// Below code will need to be utilized by the C library in userspace with a page obtained from mmap (that will be declared in kernelspace) /////////
/*#include "fsmalloc.h"
  extern page* _malloc_head;
  static int init_fsmalloc() {
    if (init_page(_malloc_head, NULL, 256, 0)) { return 1; }
    return 0;
  }*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* == (internal) set_ns
   Assign a set of pointers to a function namespace
1. Iterate over the variable list of pointers
2. If END is found, stop and return
3. If NULL is found, skip this namespace member
   * Else assign this element of the list to the current slot of the namespace
#define set_ns(_ns, _p, ...) _set_ns(_ns, _p, ##__VA_ARGS__, END)
static void _set_ns(func_ns* ns, void* p, ...) {
  va_list ap;
  void** nsp = (void**)&ns;
  mint i = 0;
  
  va_start(ap, p);
  for (;;) {
	p = va_arg(ap, void*);
	if (p == END) { break; }
	if (p) { nsp[i] = p; }
	i++;
  }
  va_end(ap);
}*/

/*func_data* lookup_func(string* func, func_ns functions, uint8_t nest) {
  func_data* f = functions;
  func_ns* ns = *(f->includes);
  while (f) {
	if (!(strcmp(func->data, f->name->data))) {
	  return f;
	}
	f = f->next;
  }
  if (nest) { return NULL; }
  while (ns) {
	if ((f = lookup_func(func, ns, 1))) {
	  return f;
	}
	ns += (sizeof(ns));
  }
  return NULL;
  }*/

/*void* phys_addr(void* virt_addr) {
  unsigned long pd_idx = (unsigned long)virt_addr >> 22;
  unsigned long pt_idx = (unsigned long)virt_addr >> 12 & 0x03FF;

  unsigned long* pd = (unsigned long*)0xFFFF000;
  if (!((pde*)pd)->present) { return NULL; }
  unsigned long* pt = ((unsigned long*)0xFFC00000) + (0x400 * pd_idx);
  if (!(*pt & 0)) { return NULL; } // check the logic here

  return (void *)(pt[pt_idx] & ~0xFFF) + ((unsigned long)virt_addr & 0xFFF);
}

void map_page(void* phys_addr, void* virt_addr, uint32_t flags) {
  unsigned long pd_idx = (unsigned long)virt_addr >> 22;
  unsigned long pt_idx = (unsigned long)virt_addr >> 12 & 0x03FF;

  unsigned long* pd = (unsigned long*)0xFFFF000;
  if (!((pde*)pd)->present) {
    (((pde*)pd)->present) = 1; 
  }
  unsigned long* pt = ((unsigned long*)0xFFC00000) + (0x400 * pd_idx);
  if (!(pt[pt_idx] & 0)) { return; } // check the logic here

  flush_tlb();
}

void unmap_page(void* phys_addr, void* virt_addr, uint32_t flags) {
  unsigned long pd_idx = (unsigned long)virt_addr >> 22;
  unsigned long pt_idx = (unsigned long)virt_addr >> 12 & 0x03FF;

  unsigned long* pd = (unsigned long*)0xFFFF000;
  if (((pde*)pd)->present) {
    (((pde*)pd)->present) = 0; 
  }
  unsigned long* pt = ((unsigned long*)0xFFC00000) + (0x400 * pd_idx);
  if ((pt[pt_idx] & 0)) { return; } // check the logic here, this should fire when there is not already a mapping present

  flush_tlb();
}

#define USERSPACE_START 0x40000000
#define PHYS_PAGES 128
uint8_t page_bitmap[PHYS_PAGES/8] __attribute__ ((aligned(4096))); //128 4MB pages == 512MB RAM minimum (and maximum used for now)

void* alloc_page() {  
  uint32_t bit;
  uint32_t i;
  for (i=0; i<PHYS_PAGES/8; i++) {
    bit = 0;
    while (bit < 8) {
      if ((page_bitmap[i]) & ~(bit << 1)) {
        bit++;
      }
      else {
        page_bitmap[i] |= (bit << 1);
        //if (already present) { ... }
        ((pde*)page_directory[bit])->addr = (void*)(bit * PAGE_SZ);
        ((pde*)page_directory[bit])->present = 1;
        flush_tlb();
        //map_page((void*)(bit * PAGE_SZ), (void*)USERSPACE_START, 0);
        return (void*)(bit * PAGE_SZ);
      }
      bit++;
    }
  }
  return NULL;
}
//== phys_addr :: Convert virtual address inside of a process to physical
void* phys_addr(void* virt_addr) {
  uint32_t pd_idx = (uint32_t)virt_addr >> 22;
  
  }

typedef uint32_t bit;
typedef struct {
  bit present : 1;
  bit rw : 1;
  bit user : 1;
  bit cache_type: 1;
  bit cache_disable :1;
  bit accessed : 1;
  bit size : 1;
  bit ignore : 1;
  bit avail : 3;
  bit addr : 21;
} pde;
uint32_t page_directory[1024] __attribute__((aligned(4096)));
//////////////////////////////////////////////////////////////
//@@@@@@@@@@@@@@@@@@@ Sycalls @@@@@@@@@@@@@@@@@@@@@//
//////////////////////////////////////////////////////////////

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
  //  errno = SYSERR_HEAP_UNINITIALIZED;
  return (void*)-1;
}

//////////////////////////////////////////////////////////////
//@@@@@@@@@@@@@@@@@@@ Static functions @@@@@@@@@@@@@@@@@@@@@//
//////////////////////////////////////////////////////////////

static void flush_tlb() {
  asm("mov %%cr3,%%eax\n"
      "mov %%eax,%%cr3"
      : : : "eax");
}

static void flush_tlb_addr(void* addr) {
  asm("invlpg %0"
      : : "m"(addr));
}

static void _enable_paging(uint32_t* pdp) {
  asm("mov %%eax, %0\n"
      "mov %%cr3, %%eax\n"
      "mov %%eax, %%cr4\n"
      "or %%eax, 0x80000010\n"
      "mov %%cr4, %%eax\n"
      : : "m"(pdp) : "eax");
}

#define STACKSIZE 0x4000
  uint32_t stack[STACKSIZE] __attribute__((section(".bss"))) __attribute__((aligned(32)));
  static void boot_up() {
  uint32_t* new_stack = 0xfa00 + STACKSIZE;
  push ebx refers to the multiboot info structure which i believe we arent using, may need to remove that ( or figur eout how to use it)
  removed both as im not sure we actually need either yet (will eventually for /proc/cmdline and such)
  "push eax\n"
  "push ebx\n"
  ---- first attempt ----
  asm("mov %%ecx, %%cr0\n"
  "or %%ecx, 0x80000000\n"
  "mov %%cr0, %%ecx\n"
  "movl $0, %0\n"
  "invlpg 0\n"
  "leal %1, %%esp\n"
  "call _main\n"
  "hlt" : : "m"(pdp), "m"(new_stack) : "esp");
  ---- second attempt ----
  asm("leal %0, %%esp\n"
  "call _main"
  : : "m"(new_stack));
  }*/
//uint8_t kram[KERNEL_MEMORY_SZ] __attribute__((aligned(4096)));
//block kram_blocks[KERNEL_MEMORY_BLK_MAX] __attribute__((aligned(4096)));
  /*vtprint(vt0, "Enabling paging ... ");
  enable_paging();
  vtprint(vt0, "[OK]\n");

  vtprint(vt0, "Configuring \"Identity Paging\" from [0x0000000:0x00000fff] ... ");
  id_paging((pde*)(page_directory[0]), 0, 131072);
  vtprint(vt0, "[OK]\n");

  vtprint(vt0, "Flushing TLB ... ");
  flush_tlb();
  vtprint(vt0, "[OK]\n");

  vtprint(vt0, "Allocating page for kernel ... ");
  uint8_t *p = (uint8_t*)alloc_page();
  vtprint(vt0, "[OK]\n"); //would be really nice to be able to use %p here on the newly retrieved page address
  vtprint(vt0, "Page fault check ... ");
  p[0] = 'a'; p[1] = 'b';
  vtprint(vt0, "[OK]\n");
  char addr[16];
  p2s(p, addr, 16);
  vtprint(vt0, addr);

  vtprint(vt0, "Allocating another page ... ");
  p = (uint8_t*)alloc_page();
  vtprint(vt0, "[OK]\n"); //would be really nice to be able to use %p here on the newly retrieved page address
  vtprint(vt0, "Page fault check ... ");
  p[0] = 'a'; p[1] = 'b';
  vtprint(vt0, "[OK]\n");
  memset(addr, 0, 16);
  p2s(p, addr, 16);
  vtprint(vt0, addr);
static void id_paging(pde* pde, void* from, size_t sz) {
  uint32_t* addr = pde->addr;
  for (from = (void*)((uint32_t)from & 0xfffff000); sz > 0; from += 4096, sz -= 4096, addr++) { *addr = (uint32_t)from|1; }
}

static void enable_paging() {
  uint32_t i;
  for (i = 0x0; i < 1024; i++) {
    page_directory[i] = 0x43;
  }
  _enable_paging(page_directory);
}
VGA_PUT(vt, c, vt->color, vt->x, vt->y);
    vt->x++;
    if (vt->x >= vt->x_max) {
    vt->x = 0;
    vt->y++;
    }
    if ((vt->y++) >= vt->y_max) {
    vt->y = 0;
    }
    vterm_update_csr(vt, vt->x, vt->y);*/
