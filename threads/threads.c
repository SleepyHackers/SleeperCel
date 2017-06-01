#define _GNU_SOURCE
#include <time.h>
#include <linux/futex.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdatomic.h>
#include <limits.h>
#include <sched.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <setjmp.h>
#include "threads.h"

/*Condition variables
  #define cnd_init(cv)							\
  atomic_init(cv, 0)*/
/*__attribute__((nonnull(1)))
static int cnd_wait(atomic_uint* cv, atomic_flag* mtx) {
        unsigned value = atomic_load_explicit(cv,
                                              memory_order_relaxed);

        while (value & 1)
        {
                if (atomic_compare_exchange_weak_explicit(cv,
                                &value, value + 1,
                                memory_order_relaxed, memory_order_relaxed))
                        value++;
        }

        mtx_unlock(mtx);
        futex_wait(cv, value, NULL);
        mtx_lock(mtx);
        return 0;
}

__attribute__((nonnull(1)))
static int cnd_signal(atomic_uint* cv) {
        atomic_fetch_or_explicit(cv, 1, memory_order_relaxed);
        futex_signal(cv);
        return 0;
		}*/

//Utils
#define make_stack(sz) ((mmap(0, sz, STACK_PFLAGS, STACK_MFLAGS, -1, 0)) + sz)
//Mutexes
#define mtx_lock(m)								\
  while (atomic_flag_test_and_set(m.f)) {		\
	futex_wait(m.x, FREE);						\
  }												
#define mtx_unlock(m)								\
  if ((atomic_flag_test_and_set(m.f))) {			\
	atomic_flag_clear(m.f);							\
    futex_wake(m.x, 1);								\
  }												
//Futexes
#define futex(addr1, op, val1, timeout, addr2, val3) syscall(__NR_futex, addr1, op, val1, timeout, addr2, val3)
#define futex_wait(addr, val)                        futex(addr, FUTEX_WAIT_PRIVATE, val, 257, NULL, 0)
#define futex_wake(addr, nr)                         futex(addr, FUTEX_WAKE_PRIVATE, nr, NULL, NULL, 0)
#define futex_signal(addr)                           ((futex_wake(addr, 1) >= 0) ? 0 : -1)
#define futex_broadcast(addr)                        ((futex_wake(addr, INT_MAX) >= 0) ? 0 : -1)
//Spin locks
#define spin_lock(_lck) while (atomic_flag_test_and_set(_lck)) { sched_yield(); usleep(2500); }
#define spin_unlock(_lck) atomic_flag_clear(_lck)

typedef struct {
  atomic_flag f;
  uint32_t x;
} mtx_t;
typedef struct {
  void* stack; 
  int tid;
  uint8_t sid;
} taskdata; //44 bits
typedef struct {
  void* stack;
  size_t sz;
} stack_record;

static atomic_flag tblxlck = ATOMIC_FLAG_INIT;
static atomic_short running = ATOMIC_VAR_INIT((short)0);
static atomic_flag tasks[MAX_THREADS];
static atomic_flag stckxlck = ATOMIC_FLAG_INIT;
static stack_record* stcktbl[MAX_THREADS] = {0};

static void waitall() {
  while (1) {
	spin_lock(&tblxlck);
	if (!(atomic_load(&running))) {
	  break;
	}
	spin_unlock(&tblxlck);
	sched_yield();
	//pause();
	wait(NULL);
	//usleep(2500);
  }
  spin_unlock(&tblxlck);
}

static void stcktbl_insert(stack_record* sp) {
  static uint8_t i;
  spin_lock(&stckxlck);
  for (i=0; i<MAX_THREADS; i++) {
	if (!stcktbl[i]) {
	  stcktbl[i] = sp;
	  break;
	}
  }
  spin_unlock(&stckxlck);
}

static void stcktbl_flush() {
  static uint8_t i;
  spin_lock(&stckxlck);
  for (i=0; i<MAX_THREADS; i++) {
	if (stcktbl[i]) {
	  munmap(stcktbl[i]->stack, stcktbl[i]->sz);
	  stcktbl[i] = NULL;
	}
  }
  spin_unlock(&stckxlck);
}

//Can NOT be macroified, must be a real function with an address for clone to enter into
__attribute__((nonnull(1)))
static void launch(intfp task) {
  static uint8_t i;
  static stack_record self;
  //Abort if a previous stack frame is found (implying that we are not running in a new thread)
  if (__builtin_frame_address(1) != NULL) {
	return;
  }
  self.stack = 	(__builtin_frame_address(0) - (STACK_MIN*32) + 16);
  self.sz    = 	(STACK_MIN*32);
	
  task();
  
  spin_lock(&tblxlck);
  i = atomic_fetch_sub(&running, (short int)1)-1;
  atomic_flag_clear(&(tasks[i]));
  spin_unlock(&tblxlck);
  stcktbl_insert(&self);
}

static uint8_t lwp(intfp task, void* stack, ssize_t stacksz, taskdata* data) {
  if (stack) { data->stack = stack; }
  else if ((data->stack = make_stack(stacksz)) == NULL) { return 1; }

  if ((data->tid = clone((intfp)launch, data->stack, SIGCHLD|CFLAGS_LIGHT, task)) == -1) { return 2; }

  spin_lock(&tblxlck);
  data->sid = (uint8_t)atomic_fetch_add(&running, (short int)1);
  if (atomic_flag_test_and_set(&(tasks[data->sid]))) { //Disallow discrepancies between runcount and taskstatus list
	_exit(3);
  }
  spin_unlock(&tblxlck);
  return 0;
}

atomic_short x = ATOMIC_VAR_INIT((short)0);
static int t1() {
  atomic_fetch_add(&x, 1);
  return 0;
}

int main() {
  for (uint8_t i = 0; i<MAX_THREADS; i++) {
	atomic_flag_clear(&(tasks[i]));
  }
  taskdata d1, d2, d3;
  uint8_t ret;
  if ((ret = lwp(t1, NULL, STACK_MIN*32, &d1))) { return ret; }
  if ((ret = lwp(t1, NULL, STACK_MIN*32, &d2))) { return ret; }
  if ((ret = lwp(t1, NULL, STACK_MIN*32, &d3))) { return ret; }
  waitall();
  stcktbl_flush();
  fprintf(stderr, "val: %d\n", atomic_load(&x));
  //  munmap(t.stack, STACK_MIN*64);
  return 0;
}

/*typedef struct {
  void* task;
  void* stack; 
  size_t sz;
  int tid;
  uint8_t sid;
  } taskdata;*/
/*mtx_t tasklocks[MAX_THREADS] = {0};
  atomic_flag taskslots[MAX_THREADS+1] = {0}; //Last slot should always be unused and initialized to zero
  atomic_flag taskstatus[MAX_THREADS];
  static int waitsid(uint8_t sid) {
  mtx_lock(&(tasklocks[sid]));
  mtx_unlock(&(tasklocks[sid]));
  }
  static int supervised_launch(taskdata* t) {
  static uint8_t i;
  static int ret;

  //this "intelligent" slot finding may in fact not be so intelligent if someone grabs the mutex before waitsid()
  for (i = 0; i != MAX_THREADS; i++) {
	if (!atomic_flag_test_and_set(&(taskslots[i]))) {
	  break;
	}
  }
  if (i == MAX_THREADS) {
	fprintf(stderr, "threads: supervised_launch: all slots in use, try again later\n");
	return 127;
  }
  t->sid = i;
  ret = ((intfp)(t->task))();
  atomic_flag_clear(&(taskslots[i]));
  }
  for (uint8_t i=0; i<MAX_THREADS; i++) {
  atomic_flag_clear(&(taskslots[i]));
  }*/
  
