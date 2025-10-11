#include "ritsu.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

typedef long (*syscall_fn_t)(long, long, long, long, long, long, long);

__hidden __thread struct pvm_vcpu_struct pvcs = { 0 };
static syscall_fn_t call_yui = NULL;

extern long jump_to_yui(long a1, long a2, long a3, long a4, long a5, long a6,
			long a7);

static inline int can_direct_nya(long a1, long a2)
{
	switch (a1) {
	/*
	 *  These syscalls may diverge the control flow and may not be handled directly.
	 *  Plus, these syscalls are not frequently used, so we can afford to use the slow path.
	 *
	 * */
	case __NR_fsync:
	case __NR_write:
	case __NR_pwrite64:
	case __NR_pwritev:
	case __NR_writev:
	case __NR_read:
	case __NR_open:
	case __NR_close:
	case __NR_sched_yield:
	case __NR_mkdir:
	case __NR_openat:
	case __NR_openat2:
	case __NR_futex_wait:
		return 1;
	default:
		return 0;
	}
}

void __hidden check_kernel_rsp(u64 rsp)
{
	if (rsp < 0xffff000000000000) {
		while (1)
			;
	}
}

void __hidden check_user_rsp(u64 rsp)
{
	if (rsp > 0xffff000000000000) {
		while (1)
			;
	}
}

void __hidden check_pvcs(u64 pvcs_res)
{
	while (pvcs_res != (unsigned long)&pvcs)
		;
}

static long tsumugi(long a1, long a2, long a3, long a4, long a5, long a6,
		    long a7)
{
	long ret;

	if (likely(can_direct_nya(a1, a2))) {
		if (unlikely(!pvcs.reserved1)) 
			call_yui(__SYS_PVCS_SET_TLS, (unsigned long)(&pvcs), 0,
			0, 0, 0, 0);
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
