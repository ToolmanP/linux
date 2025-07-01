#include "ritsu.h"
#include <stdio.h>

typedef long (*syscall_fn_t)(long, long, long, long, long, long, long);

__thread struct pvm_vcpu_struct pvcs = { 0 };
__thread bool pvcs_initialized = false;
__attribute__((visibility("hidden"))) unsigned long yui_direct = 0;
static syscall_fn_t call_yui = NULL;

extern long jump_to_yui(long a1, long a2, long a3, long a4, long a5, long a6,
			long a7);

static inline int can_direct_nya(long a1)
{
	switch (a1) {
	/*
	 *  These syscalls may diverge the control flow and may not be handled directly.
	 *  Plus, these syscalls are not frequently used, so we can afford to use the slow path.
	 *
	 * */
	case __NR_fork:
	case __NR_vfork:
	case __NR_clone:
	case __NR_clone3:
	case __NR_execve:
	case __NR_execveat:
	case __NR_exit:
	case __NR_exit_group:
		return 0;
	default:
		return 1;
	}
}

static long tsumugi(long a1, long a2, long a3, long a4, long a5, long a6,
		    long a7)
{
	if (likely(can_direct_nya(a1))) {
		if (unlikely(!pvcs_initialized)) {
			pvcs_initialized = true;
			syscall(__SYS_PVCS_SET_TLS, (unsigned long)(&pvcs));
		}
		yui_direct = syscall(__SYS_YUI_DIRECT);
		return jump_to_yui(a1, a2, a3, a4, a5, a6, a7);
	} else {
		return call_yui(a1, a2, a3, a4, a5, a6, a7);
	}
}

int __hook_init(long placeholder __attribute__((unused)),
		void *sys_call_hook_ptr)
{
	call_yui = *((syscall_fn_t *)sys_call_hook_ptr);
	*((syscall_fn_t *)sys_call_hook_ptr) = tsumugi;
	return 0;
}
