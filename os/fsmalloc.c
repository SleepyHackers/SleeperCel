#define _GNU_SOURCE
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <string.h>
#include "types.h"
#include "fsmalloc.h"

#define toggle(n) (((n) ? (n--) : (n++)) + 1)
#define max(a,b) (a > b ? a : b)

#define FREE 0
#define USED 1
#define RESERVED 2
#define F1 "\e[38;5;228m"
#define F2 "\e[38;5;195m"
#define F3 "\e[0m"
#define F4 "\e[38;5;219m"

static page* _malloc_search_page(page* h);
static void update(page* p);

page _malloc_static_page = {0};
page* _malloc_head = &_malloc_static_page;
uint8_t left  = 0;
uint8_t right = 1;
uint8_t g = 0;
FILE* dbg;

#define xchg(_x, _y)												\
  _x = (page*)((mintptr)_x ^ (mintptr)_y);							\
  _y = (page*)((mintptr)_x ^ (mintptr)_y);							\
  _x = (page*)((mintptr)_x ^ (mintptr)_y);														
#define reorder(_a, _b)													\
  if ((order[_a] ? order[_a]->max : 0) > (order[_b] ? order[_b]->max : 0)) { \
	xchg(order[_a], order[_b]);											\
  }																	
#define psz(p)  (p ? p->max : 0)
#define porf(p) (p ? p : (page*)0xffffffffffff)
#define pfmt(p) (p ? F2 : F4)
#define dbg_order_n()							\
	  fprintf(stderr, "[debug] order_tree: starting point: %s[%-14p]%s\n" \
                   	  "                                    /\n"						\
		              "                       %s[%-14p]%s\n"							\
		              "                        /           \\\n"						\
			          "            %s[%-14p]%s %s[%-14p]%s\n",					\
			  pfmt(n->parent), porf(n->parent), F3, F1, porf(n), F3, pfmt(n->child[left]), porf(n->child[left]), F3, pfmt(n->child[right]), porf(n->child[right]), F3);
#define dbg_order_step(s)						\
	  fprintf(stderr, "[debug] order_tree: order[0]->parent %s[%-14p]%s <%d> :: %s :: %p %p %p\n" \
         			  "                                     /           \n"     \
		              "              order[0]  %s[%-14p]%s <%d>\n"           \
		              "                         /           \\\n"               \
		              "        <%d> %s[%-14p]%s %s[%-14p]%s <%d>\n",       \
			  pfmt(order[0]->parent), porf(order[0]->parent), F3, psz(order[0]->parent), s,	order[0], order[1], order[2], \
			  F1, porf(order[0]), F3, psz(order[0]),					\
			  psz(order[0]->child[left]), pfmt(order[0]->child[left]), porf(order[0]->child[left]), F3, \
			  pfmt(order[0]->child[right]), porf(order[0]->child[right]), F3, psz(order[0]->child[right]));

	/*
input state:
        A    
       / 
 n == B   
     / \
    D   C

calculated order: 
  {C, B, D}

result of reordering:
       A
      /
     C
    / \ -- children are unordered, only the maximum matters
   D   B
	*/
static void* order_tree(page* p) {
  page* o;
  page* n = p;
  page* order[3];

  fprintf(stderr, "[debug] order_tree: <begin>\n");
  while (n) {
	fprintf(stderr, "[debug] order_tree: page [%p]\n", n);
	fprintf(stderr, "\n\n\n>>>>>>>>>>>>>>>>>>>>>>> N == %p <<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
			              ">>>>>>>>>>>>>>>>>>>>>>> N -> PARENT == %p <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
			              ">>>>>>>>>>>>>>>>>>>>>>> N -> LEFT == %p <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
			">>>>>>>>>>>>>>>>>>>>>>> N -> RIGHT == %p <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n", n, n->parent, n->child[left], n->child[right]);
	order[0] = n;
	order[1] = n->child[left];
	order[2] = n->child[right];

	fprintf(stderr, "%s[debug] order_tree: pre-order for  [%-14p] -> [%-14p]: %p %p %p\n%s", F1, n->parent, n, order[0], order[1], order[2], F3);
	reorder(1, 0);
	fprintf(stderr, "%s[debug] order_tree: order step:%-42s%p %p %p\n%s", F2, "", order[0], order[1], order[2], F3);
	reorder(2, 1);
	fprintf(stderr, "%s[debug] order_tree: order step:%-42s%p %p %p\n%s", F2, "", order[0], order[1], order[2], F3);
	reorder(1, 0);
	fprintf(stderr, "%s[debug] order_tree: order step:%-42s%p %p %p\n%s", F2, "", order[0], order[1], order[2], F3);
	
	if (order[0] == n) {
	  fprintf(stderr, "[debug] order_tree: post-order and pre-order maximums are equivalent, not reordering this node\n", n, order[0], order[1], order[2]);
	}
	else {
	  fprintf(stderr, "[debug] order_tree: reordering page [%p]\n", n);
	  dbg_order_n();
	  dbg_order_step("start");
	  order[0]->parent = n->parent;
	  order[0]->child[left] = order[2];
	  order[0]->child[right] = order[1];
	  dbg_order_step("initial reassignment");

	  if (n->parent) {
		if (n->parent->child[left] == n) {
		  n->parent->child[left] = order[0];
		}
		else {
		  n->parent->child[right] = order[0];
		}
	  }
	  dbg_order_step("parent");

	  if (order[1]) {
		if (order[1]->parent) {
		  if (order[1]->parent->child[left] == order[1]) {
			fprintf(stderr, "         9               9             9        im running out of ways to make easily visible messages here <<<< 1 >>>>\n");
			order[1]->parent->child[left] = NULL;
		  }
		  else if (order[1]->parent->child[right] == order[1]) {
			fprintf(stderr, "         9               9             9        im running out of ways to make easily visible messages here <<<< 2 >>>>\n");
			order[1]->parent->child[right] = NULL;
		  }
		}
		order[1]->parent = order[0];
		if (order[0] == order[1]->child[left] || order[2] == order[1]->child[left]) {
		  fprintf(stderr, "         9               9             9        im running out of ways to make easily visible messages here <<<< 3 >>>>\n");
		  order[1]->child[left] = NULL;
		}
		else if (order[0] == order[1]->child[right] || order[2] == order[1]->child[right]) {
		  fprintf(stderr, "         9               9             9        im running out of ways to make easily visible messages here <<<< 4 >>>>\n");
		  order[1]->child[right] = NULL;
		}
	  }
	  dbg_order_step("swap 1");
	  
	  if (order[2]) {
		if (order[2]->parent) {
		  if (order[2]->parent->child[left] == order[2]) {
			fprintf(stderr, "         9               9             9        im running out of ways to make easily visible messages here <<<< 5 >>>>\n");
			order[2]->parent->child[left] = NULL;
		  }
		  else if (order[2]->parent->child[right] == order[2]) {
			fprintf(stderr, "         9               9             9        im running out of ways to make easily visible messages here <<<< 6 >>>>\n");
			order[2]->parent->child[right] = NULL;
		  }
		}
		order[2]->parent = order[0]; 
		if (order[0] == order[2]->child[left] || order[1] == order[2]->child[left]) {
		  fprintf(stderr, "         9               9             9        im running out of ways to make easily visible messages here <<<< 7 >>>>\n");
		  order[2]->child[left] = NULL;
		}
		else if (order[0] == order[2]->child[right] || order[1] == order[2]->child[right]) {
		  fprintf(stderr, "         9               9             9        im running out of ways to make easily visible messages here <<<< 8 >>>>\n");
		  order[2]->child[right] = NULL;
		}
	  }

	  dbg_order_step("swap 2");
	  
	  // * //
	  for (uint8_t f=0; f<2; f++) { //might need to move this block to after the loop
		if (order[0]->child[f]) {
		  fprintf(stderr, "\e[38;5;229mHELLO WORLD\e[0m\n");
		  if (order[0]->child[f]->child[left] == order[0]) {
			fprintf(stderr, "\e[38;5;200mHELLO WORLD\e[0m\n");			 
			order[0]->child[f]->child[left] = NULL;
		  }
		  else if (order[0]->child[f]->child[right] == order[0]) {
			fprintf(stderr, "\e[38;5;170mHELLO WORLD\e[0m\n");
			order[0]->child[f]->child[right] = NULL;
		  }
		}
	  }
	  // * //

	  dbg_order_step("bamboozling");
	}
	if (!order[0]->parent) { break; }
	else                   { n = order[0]->parent; }
  }

  fprintf(stderr, "[debug] order_tree <end> order[0]: %p\n", order[0]);
  return order[0];
}

/*
== init_page() Initialize and populate a new page struct
1. Align size of heap and block list to 4096 (system page size, required by mmap)
2. Map and assign heap
3. Map and assign block list
4. Populate remaining fields
 */
int init_page(page* p, page* parent, mint blk_max, mint heap_sz) {
  mint hsz = heap_sz + sizeof(page);
  mint sz = blk_max * sizeof(block);

  //The amount of padding we have to use for the block lists is pretty bad (sizeof(block) == 17)
  while (sz % 4096)  { sz++;  }
  while (hsz % 4096) { hsz++; }

  if ((p->addr = mmap(NULL, hsz, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS|MAP_POPULATE, -1, 0)) == (void*)-1) {
	fprintf(stderr, "[error] init_page(): mmap failed: %s\n~~have you ever wondered why error messages are so boring? its because they want you to be robots like them\n", strerror(errno));
	return 1;
  }

  if ((p->blk_list = mmap(NULL, sz, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS|MAP_POPULATE, -1, 0)) == (void*)-1) {
	fprintf(stderr, "[error] init_page(): mmap failed: %s\n", strerror(errno));
	return 1;
	}

  p->max = hsz;
  p->blk_list->addr = p->addr;
  p->blk_list->sz = hsz;
  p->blk_max = blk_max; 
  p->heap_sz = hsz;
  p->parent = parent;
  return 0;
}

/*
== (internal) Release page and it's associated resources
1. Unmap page heap
2. Unmap page block list
3. Remove pointer to self from parent
4. Free page struct 
 */
static void destroy_page(page* p, page* h) {
  page* me;
  page* new;
  uint8_t f = 0;
  if (p->parent) {
	if (p->parent->child[left] == p) {
	  p->parent->child[left] = NULL;
	}
	else if (p->parent->child[right] == p) {
	  p->parent->child[right] = NULL;
	}
  }
  else {
	if      (p->child[left])  { _malloc_head = p->child[left];  }
	else if (p->child[right]) { _malloc_head = p->child[right]; }
	else {
	  update(_malloc_head);
	  fprintf(stderr, " ~~~~~~~~~~~~~~ [smoke] ~~~~~~~~~~~~~~\n");
	  return;
	}
	_malloc_head->parent = NULL;
	memset(p, 0, sizeof(page));
	_free(p);
	update(_malloc_head);
	_malloc_head = order_tree(_malloc_head);
	h = _malloc_head;
	fprintf(stderr, "bitch tits #@!#@!#@!#@!#@!#@! #@!#@! (0)(0)(0) _malloc_head [%p] <><><><>   #@!#@!#@!#@!#@!#@!            %%%%%%%%\n", _malloc_head);
	return;
  }
  if (!p->heap_sz) {
	return;
  }
  if (munmap(p->addr, p->heap_sz) == -1) {
	fprintf(stderr, "[error] destroy_page: couldn't unmap %d-byte heap @ %p : %s\n",
			p->heap_sz, p->addr, strerror(errno));
	return;
  }
  if (munmap(p->blk_list, p->blk_max*sizeof(block)) == -1) {
	fprintf(stderr, "[error] destroy_page: couldn't unmap %d-byte block list @ %p : %s\n",
			p->heap_sz, p->addr, strerror(errno));
	return;
  }
  //We need to move any populated children to other endpoints
  if (p->child[left]) {
	new = _malloc_search_page(h);
	fprintf(stderr, "@@@@@@ destroy_page :::::::::::: new == \e[1m[%p]\e[0m p == \e[1m[%p]\e[0m\n", new, p);
	if (!new) {
	  fprintf(stderr, "[error] destroy_page: couldn't find a free endpoint in the tree\n");
	  return;
	}

	if (new->child[f]) { toggle(f); }
	new->child[f] = p->child[left];
	new->child[f]->parent = new;
	update(new->child[f]);
	_malloc_head = order_tree(new->child[f]);
	h = _malloc_head;
  }
  if (p->child[right]) {
	new = _malloc_search_page(h);
	fprintf(stderr, "@@@@@@ destroy_page :::::::::::: new == \e[1m[%p]\e[0m p == \e[1m[%p]\e[0m\n", new, p);
	if (!new) {
	  fprintf(stderr, "[error] destroy_page: couldn't find a free endpoint in the tree\n");
	  return;
	}

	if (new->child[f]) { toggle(f); }
	new->child[f] = p->child[right];
	new->child[f]->parent = new;
	update(new->child[f]); 
	_malloc_head = order_tree(new->child[f]);
	h = _malloc_head;
  }
  memset(p, 0, sizeof(page));
  _free(p);
}

/*
== (internal) Update
1. Recompute p->max 
*/
static void update(page* p) {
  mint v=0;
  block* b = p->blk_list;
  while (b) {
	if (!b->used && b->sz > v) { v = b->sz; }
	b = b->next;
  }
  if (v != p->max) { p->max = v; }
}

/*
== (internal) Malloc from page
1. Traverse page block list until an element is found where (used == 0) && (requested size <= size of block)
2. Return NULL if we've hit the sentinel value, indicating no available blocks in this page at this size
3. Iterate the block array until a free space is found
4. Return NULL if we've hit the block count maximum, indicating no available blocks in this page at all
5. Fragment the page
*/
static void* _malloc_page (page* pg, mint sz) {
  block* f;
  block* p = pg->blk_list;
  mint i = 0;
  while (p) {
	if ((!p->used) && (sz <= p->sz)) {
	  break;
	}
	p=p->next;
  }
  if (!p) { return NULL; }
  for (i=0; i<pg->blk_max; i++) {
	if (!pg->blk_list[i].addr) { break; }
  }
  if (i >= pg->blk_max) {
	return NULL;
  }
  fprintf(stderr, "[debug] _malloc_page: page %p page->max %d\n", pg, pg->max);
  f = &(pg->blk_list[i]);
  f->addr = p->addr+sz+1;
  f->sz = p->sz - sz;
  p->sz = sz;
  p->used = 1;
  p->next = f;
  f->prev = p;
  return p->addr;
}
/*
== Malloc
1. [path A] If (size > _malloc_head->max) then we need to create a new page:
   * Walk from _malloc_head to a random endpoint
   * Invoke self to allocate enough space for new page struct
     >>NOTE: should probably reserve sizeof(page) at end of page heap
             to ensure we always have enough space for a page struct (and can thus expand the tree)
   * Initialize new page
2. [path B] Else a page (or pages) exists in the tree with a free block >= our size:
   * Walk from _malloc_head while (page->child->max >= size)
3. Invoke _malloc_page(page, size)
4. Subtract newly allocated size from p->max
5. If (size == p->max) then update maximums
//  p->res = p->addr+hsz+1; //Start of reserved slot for new page struct when tree is being expanded
*/


static page* _malloc_search_page_max_internal(page* h, mint sz, uint8_t f) {
 page* p = h;
 page* r;
 fprintf(stderr, "[debug] _malloc_search_page_max_internal: <begin> page [%p]\n", p);
 toggle(f);
 if ((p->child[f]) && (p->child[f]->max >= sz)) {
   fprintf(stderr, "[debug] _malloc_search_page_max_internal: <left> [%p]\n", p->child[f]);
   if ((r =  _malloc_search_page_max_internal(p->child[f], sz, f))) {
	 return r;
   }
 }
 if ((toggle(f)) && (p->child[f]) && (p->child[f]->max >= sz)) {
   fprintf(stderr, "[debug] _malloc_search_page_max_internal: <right> [%p]\n", p->child[f]);
   if ((r = _malloc_search_page_max_internal(p->child[f], sz, f))) {
	 return r;
   }
 }
 fprintf(stderr, "[debug] _malloc_search_page_max_internal: <end> page [%p] page->max [%d]\n", p, p->max);
 return p;
}

static page* _malloc_search_page_max(page* h, mint sz) {
  uint8_t f = left;
  return _malloc_search_page_max_internal(h, sz, f);
}

static page* _malloc_search_page_internal(page* h, uint8_t f) {
  page* p = h;
  page* r;
  
  toggle(f);

  if ((!p->child[left]) || ((!p->child[right]))) {
	fprintf(stderr, "[debug] _malloc_search_page_internal: <A>\n");
	return p;
  }
  if (p->child[f])  {
	fprintf(stderr, "[debug] _malloc_search_page_internal: <B>\n");
	if ((r = _malloc_search_page_internal(p->child[f], f))) {
	  return r;
	}
  }
  if (toggle(f) && p->child[f]) {
	fprintf(stderr, "[debug] _malloc_search_page_internal: <end> %p\n", p);
	if ((r =  _malloc_search_page_internal(p->child[f], f))) {
	  return r;
	}
  }

  fprintf(stderr, "[debug] _malloc_search_page: <D> p [%p]  p->max [%d]\n", p, p->max);
  return NULL;
}

static page* _malloc_search_page(page* h) {
  uint8_t f = right;
  return _malloc_search_page_internal(h, f);
}

void* __wrap_malloc(mint sz) {
  uint8_t f = 0;
  page* p = _malloc_head;
  void* r;

  if (p->max < sizeof(page)) {
	fprintf(stderr, "[error] malloc: not enough space for a new page struct\n");
	return NULL;
  }
  if (sz > (p->max - sizeof(page))) { ////path A
	fprintf(stderr, "[debug] malloc: <path A> entered with: sz [%d] p->max [%d]\n", sz, p->max);
	if (sizeof(page) > p->max) {
	  fprintf(stderr, "[error] malloc: couldn't find enough space for a new page structure\n");
	  return NULL;
	}
	fprintf(stderr, "[debug] malloc: <path A -> path B> \n");
	page* n = __wrap_malloc(sizeof(page));
	fprintf(stderr, "[debug] malloc: <path A> resumed with: sz [%d] p->max [%d]\n", sz, p->max);
	if (!(p = _malloc_search_page(p))) {
	  fprintf(stderr, "[error] malloc: <path A> tree cannot be expanded at this time\n");
	  return NULL;
	}
	fprintf(stderr, "[debug] malloc: <path A> [0] n->max %d\n", n->max);
	if (init_page(n, p, 256, sz)) {
	  fprintf(stderr, "[error] malloc: <path A> couldn't initialize page\n");
	  return NULL;
	}
	if (p->child[f]) { toggle(f); }
	fprintf(stderr, "[debug] malloc: <path A> [p] %p [child] %p [f] %d\n", p, p->child[f], f);
	p->child[f] = n; // [return]
	p = n;
	
	fprintf(stderr, "[debug] malloc: <path A> [1] p [%p] p->max [%d] sz [%d]\n", p, p->max, sz);
	if (p->max == p->heap_sz) { p->max -= sz; }
	fprintf(stderr, "[debug] malloc: <path A> [2] p [%p] p->max [%d] sz [%d]\n", p, p->max, sz);
  }
  else { //// path B
	fprintf(stderr, "[debug] malloc: <path B> entered with: sz [%d] p->max [%d]\n", sz, p->max);
	if (!(p = _malloc_search_page_max(p, sz))) {
	  fprintf(stderr, "[error] malloc: <path B> tree cannot be expanded at this time\n");
	  return NULL;
	}
	fprintf(stderr, "[debug] malloc: <path B> p->max %d\n", p->max);
	p->max -= sz;
	fprintf(stderr, "[debug] malloc: <path B> p->max %d\n", p->max);
  }
  if (!(r = _malloc_page(p, sz))) {
	fprintf(stderr, "[error] malloc: _malloc_page failed\n");
	return NULL;
  }
  fprintf(stderr, "[debug] malloc: <common> entered\n");
  fprintf(stderr, "[debug] malloc: <common> p->max %d\n", p->max);
  if (sz > p->max) { update(p); }
  _malloc_head = order_tree(p);
  fprintf(stderr, "[debug] malloc: <common> p->max %d\n", p->max);
  return r;
}

/*
== (internal) Search page tree for the node responsible for address
*/
static page* _free_search(page* p, void* addr) {
  page* r;
  if ((p->addr <= addr) && (p->addr+p->heap_sz >= addr)) {
	return p;
  }
  fprintf(stderr, ">> p->addr [%p] p->heap_sz [%d] p->addr+p->heap_sz [%d]\n", p->addr, p->heap_sz, p->addr+p->heap_sz);
  if (p->child[left])  {
	if ((r = _free_search(p->child[left], addr))) {
	  return r;
	}
  }
  if (p->child[right]) {
	if ((r = _free_search(p->child[right], addr))) {
	  return r;
	}
  }
  return NULL;
}

/*
== (internal) Free address from within page
1. Iterate the page block list until the desired block->addr is found
2. Return if the address couldn't be found
3. If there's free space to the right, merge it with our block and zero the unneeded struct
4. If there's free space to the left, shift down and repeat step 3.
5. Clear block used flag (set to FREE)
6. If (page != _malloc_head) && (block->size == page->heap_sz) then release the unused page and it's associated resources
7. If (block->size > page->max) then update(page)
 */
static void _free_block(page* pg, void* addr) {
  block* p = &pg->blk_list[0];
  block* old;
  while (p) {
	if ((p->used) && (p->addr == addr)) { break; }
	p=p->next;
  }
  if (!p) { //Nop if address couldn't be found in block list
	fprintf(stderr, "[error] free: nop @ %p\nyou may have a memory leak, be sure to download more RAM immediately\n", addr);
	return;
  }
  fprintf(stderr, "[debug] free: p [%p] addr [%p] block [%p]\n", pg, addr, p);
  
  if ((p->next) && (!p->next->used)) { //Merge right
	fprintf(stderr,"[debug] free: right merge: %p %p\n", p->addr, p->next->addr);
	p->sz += p->next->sz;
	old = p->next;
	p->next = p->next->next;
	if (p->next) { p->next->prev = p; }
	memset(old, 0, sizeof(block));
  }
  if ((p->prev) && (!p->prev->used)) { //Shift left, then merge right
	fprintf(stderr,"[debug] free: left merge: %p %p\n", p->prev->addr, p->addr);
	p = p->prev;
	p->sz += p->next->sz;
	old = p->next;
	p->next = p->next->next;
	if (p->next) { p->next->prev = p; }
	memset(old, 0, sizeof(block));
  }
  p->used = 0;
  if (p->sz == pg->heap_sz) {
	return destroy_page(pg,_malloc_head);
  }
  else {
	fprintf(stderr, "********* @@@@@@@@@@@@@@@ DID ******* NOT ********* DESTROY p [%p] p->sz [%d] @@@@@@@@@@@@@@**********\n\n\n\n", p, p->sz);
    memset(p->addr, 0, p->sz); //We don't need to zero destroyed pages because new MAP_ANONYMOUS mmaped regions are zeroed by the kernel
  }
  if (p->sz > pg->max) {
	pg->max = p->sz;
	update(pg);
  }
  _malloc_head = order_tree(pg);
}

/*
== Free
1. Search tree for block responsible for addr
2. Free block
*/
void __wrap_free(void* addr) {
  page* p = _free_search(_malloc_head, addr);
  if (!p) {
	fprintf(stderr, "[error] free: couldn't locate page for address %p\n", addr);
	return;
  }
  _free_block(p, addr);
}

static void dump_page(page* p) {
  block* b = p->blk_list;
  char* state;
  fprintf(stderr, "%s[page @%p]\e[0m\n", F1, p);
  fprintf(stderr, "%s    |\t%s%-8s %-12s %-16s %-16s %-16s %-16s\e[0m\n", F1, F2, "[max]", "[blk_max]", "[heap_sz]", "[left]", "[parent]", "[right]");
  fprintf(stderr, "%s    |\t%s%-8d %-12d %-16d %-16p %-16p %-16p\e[0m\n", F1, F3, p->max, p->blk_max, p->heap_sz, p->child[left], p->parent, p->child[right]);
  fprintf(stderr, "%s    \\---[block list @%p]\e[0m\n", F1, p->blk_list);
  fprintf(stderr, "\t\t%s%-16s %-8s %-16s %-16s %-4s\e[0m\n", F4, "[address]", "[size]", "[next]", "[prev]", "[state]");
  while (b) {
	switch(b->used) {
	case FREE:     state = "FREE";     break;
	case USED:     state = "USED";     break;
	default:       state = NULL;       break;
	}
	fprintf(stderr, "\t\t%s%-16p %-8d %-16p %-16p %-4s\e[0m\n", F3, b->addr, b->sz,
			(b->next) ? b->next->addr : NULL,
			(b->prev) ? b->prev->addr : NULL,
			state);
	b=b->next;
  }
}

static void _dump_tree(page* p) {
  dump_page(p);
  if (p->child[left])  { _dump_tree(p->child[left]); }
  if (p->child[right]) { _dump_tree(p->child[right]); }
}

void dump_tree(page* p) {
  _dump_tree(p);
  putc('\n', stderr);
}

static char* dump_page2(char* buf, mint sz, page* p) {
  block* b = p->blk_list;
  char* s = buf;
  char* state;
  
  s += snprintf(buf, sz, "[page @%p]\nmax:%19d\nblk_max:%15d\nheap_sz:%15d\nleft:%18p\nparent:%16p\nright:%17p\n",
		  p, p->max, p->blk_max, p->heap_sz, p->child[left], p->parent, p->child[right]);
  s += snprintf(s, (sz - (s - buf)), "%s%-16s %-8s %-16s %-16s %-4s\e[0m\n", F2, "[address]", "[size]", "[next]", "[prev]", "[state]");
  
  while (b) {
	switch(b->used) {
	case FREE:     state = "FREE";     break;
	case USED:     state = "USED";     break;
	default:       state = NULL;       break;
	}
	s += snprintf(s, (sz - (s - buf)), "%s%-16p %-8d %-16p %-16p %-4s\e[0m\n",
				  F3, b->addr, b->sz,
				  (b->next) ? b->next->addr : NULL,
				  (b->prev) ? b->prev->addr : NULL,
				  state);
	b=b->next;
  }

  if (s == buf) { return NULL; }
  return s;
}

static void _bfs_dump_stdout(page* p) {
  dump_page(p);
}

typedef struct {
  page* p;
  mint level;
} _page_sort;

static mint _dfs_xml(FILE* fp, page* p, _page_sort* list, mint i, mint height, mint level) {
  _page_sort d = {p, level};
  block* b = p->blk_list;
  mint x;
  char tabs[height];
  char* tabp, * ntype;
  tabs[height-1] = 0;
  tabp = &(tabs[height-1]); 
  for (x = 0; x < level; x ++) {
	tabp--;
	*tabp = '\t';
  }

  if (!p->parent)                                   { ntype = "_malloc_head"; }
  else if ((!p->child[left]) && (!p->child[right])) { ntype = "edge"; }
  else                                              { ntype = "node"; }
  
  fprintf(fp, "%s\t<%s level=\"%d\" addr=\"%p\" max=\"%d\" blk_max=\"%d\" heap_sz=\"%d\" left=\"%p\" parent=\"%p\" right=\"%p\">\n",
		  tabp, ntype, level, p, p->max, p->blk_max, p->heap_sz, p->child[left], p->parent, p->child[right]);
  while (b) {
	fprintf(fp, "%s\t\t<block addr=\"%p\" sz=\"%d\" next=\"%p\" prev=\"%p\" state=\"%s\"/>\n",
			tabp, b->addr, b->sz,
			((b->next) ? b->next->addr : NULL),
			((b->prev) ? b->prev->addr : NULL),
			((b->used) ? "USED" : "FREE"));
	b = b->next;
  }

  memcpy(&list[i], &d, sizeof(_page_sort));
  level++; i++;
  if (list[i].p == (page*)0xFFFFFFFF) {
	fprintf(stderr, "[warn] _bfs_dfs: ran out of space in the node list\n");
	return i;
  }
  if (p->child[left]) { i = _dfs_xml(fp, p->child[left], list, i, height, level); }
  if (p->child[right]) { i = _dfs_xml(fp, p->child[right], list, i, height, level); }
  fprintf(fp, "%s\t</%s>\n", tabp, ntype);
  return i;
}

static void bfs_walk(_page_sort* list, mint height, mint length) {
  mint i;
  mint l;
  fprintf(stderr, "[debug]: bfs_walk: <begin>\n");
  for (l=0; l<height; l++) {
	for (i=0; i<length; i++) {
	  if (list[i].p && list[i].p != (page*)0xFFFFFFFF && list[i].level == l) {
		fprintf(stderr, "** %p %d **\n", list[i].p, list[i].level);
	  }
	}
  }
}

static void _malloc_dump_xml(FILE* fp, page* h, mint maxheight, mint maxlength) {
  page* p = h;
  mint length;
  mint height=0;
  _page_sort list[maxlength];
  memset(list, 0, (sizeof(_page_sort) * maxlength));
  list[maxlength-1].p = (page*)0xFFFFFFFF;

  _dfs_xml(fp, p, list, 0, maxheight, 0);

  for (length=0; length<maxlength; length++) {
	if (!list[length].p) { break; }
	if (list[length].level > height) { height = list[length].level; }
	fprintf(stderr, "** %p %d **\n", list[length].p, list[length].level);
  }
  bfs_walk(list, height+1, length); 
}

static void _malloc_begin_xml(char* file, FILE** fp) {
  if (!(*fp = fopen(file, "w"))) {
	fprintf(stderr, "[warn] couldn't open the XML debug output file \"%s\": %s\n", file, strerror(errno));
	return;
  }
  fprintf(*fp, "<dump>\n");  
}

static void _malloc_end_xml(FILE* fp) {
  fprintf(fp, "</dump>\n");
  fclose(fp);
}

