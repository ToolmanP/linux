#include "ritsu.h"
#include <stdlib.h>
#include <stdatomic.h>
#include <stdio.h>

typedef long (*syscall_fn_t)(long, long, long, long, long, long, long);

static syscall_fn_t call_yui = NULL;
static atomic_int thread_idx = 1;

extern long jump_to_yui(long a1, long a2, long a3, long a4, long a5, long a6,
			long a7);

static int direct_call_table[459] = {
  [__NR_open] = 1,
  [__NR_openat2] = 1,
  [__NR_write] = 1,
  [__NR_writev] = 1,
  [__NR_pwritev] = 1,
  [__NR_pwritev2] = 1,
  [__NR_pwrite64] = 1,
  [__NR_read] = 1,
  [__NR_readv] = 1,
  [__NR_pread64] = 1,
  [__NR_preadv] = 1,
  [__NR_preadv2] = 1,
  [__NR_close] = 1,
  [__NR_fallocate] = 1,
  [__NR_lseek] = 1,
  [__NR_fsync] = 1,
  [__NR_sync] = 1,
};

static __always_inline unsigned long rdgsbase(void)
{
	unsigned long gsbase;

	asm volatile("rdgsbase %0" : "=r"(gsbase)::"memory");

	return gsbase;
}

static long tsumugi(long a1, long a2, long a3, long a4, long a5, long a6,
		    long a7)
{
	long ret;
  unsigned long value = rdgsbase();

	if ((value & GSBASE_MAGIC) != GSBASE_MAGIC) {
    value = (unsigned long)(atomic_fetch_add(&thread_idx, 1) * 4096UL);
		call_yui(__SYS_PVCS_SET_TLS, value, 0, 0, 0, 0,0);
	}

	if (direct_call_table[a1] && (rdgsbase() & GSBASE_MAGIC) == GSBASE_MAGIC) {
		ret = jump_to_yui(a1, a2, a3, a4, a5, a6, a7);
	} else {
		ret = call_yui(a1, a2, a3, a4, a5, a6, a7);
	}

	return ret;
}

int __hook_init(long placeholder __attribute__((unused)),
		void *sys_call_hook_ptr)
{
	call_yui = *((syscall_fn_t *)sys_call_hook_ptr);

	*((syscall_fn_t *)sys_call_hook_ptr) = tsumugi;
	return 0;
}
