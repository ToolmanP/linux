/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_X86_RUNPV_PARA_H
#define __ASM_X86_RUNPV_PARA_H

#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/pvm_para.h>

#ifndef __ASSEMBLY__

#ifdef CONFIG_RUNPV_GUEST
DECLARE_PER_CPU_PAGE_ALIGNED(struct pvm_vcpu_struct, pvm_vcpu_struct);
void runpv_setup_pvcs(int cpu);
int runpv_mark_remap_pvcs_tls(struct task_struct *tsk);
void entry_DIRECTCALL_64_runpv(void);


static inline long runpv_hypercall3(long nr, long a0, long a1, long a2)
{
	long ret;
	register long r10_reg asm("r10") = a2;
	asm volatile("call runpv_hypercall"
		     : "=a"(ret), "+D"(nr), "+S"(a0), "+d"(a1), "+r"(r10_reg)
		     :
		     : "memory", "rcx", "r8", "r9", "r11", "r12", "r13", "r14", "rbx", "rbp", "r15");
	if (ret == -ENOSYS) {
		pr_info("NOSYS: nr %lx\n", nr);
	}
	return ret;
}

static inline long runpv_hypercall2(long nr, long a0, long a1)
{
	return runpv_hypercall3(nr, a0, a1, 0);
}

static inline long runpv_hypercall1(long nr, long a0)
{
	return runpv_hypercall2(nr, a0, 0);
}

static inline long runpv_hypercall0(long nr)
{
	return runpv_hypercall1(nr, 0);
}

void __init runpv_early_setup(void);

#else

static inline void runpv_setup_pvcs(int cpu) { }

static inline void runpv_early_setup(void) {

}

static inline void runpv_mark_remap_pvcs_tls(struct task_struct *tsk) {

}


#endif

#ifdef CONFIG_RUNPV_MEM_PARAVIRT
void runpv_free_page_hook(struct page *page, unsigned int order);
void runpv_alloc_page_hook(struct page *page, unsigned int order, gfp_t gfp_flags);
#else
static inline void runpv_free_page_hook(struct page *page, unsigned int order) {

}

static inline void runpv_alloc_page_hook(struct page *page, unsigned int order, gfp_t gfp_flags){

}
#endif

#endif
#endif
