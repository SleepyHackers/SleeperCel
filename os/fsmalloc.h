#pragma once

#include "types.h"

typedef struct page page;
typedef struct block block;
struct block {
  void* addr;
  mint sz;
  block* next;
  block* prev;
  uint8_t used;
};
struct page {
  mint max; //Size of largest free block in page;
  block* blk_list;
  mint blk_max; //Max number of blocks in the list
  void* addr; //Starting address of data
  mint heap_sz; //Size of data space in bytes
  page* parent;
  page* child[2];
};

void* _malloc(mint sz);
void _free(void* addr);
int init_page(page* p, page* parent, mint blk_max, mint heap_sz);
void dump_tree(page* p);
