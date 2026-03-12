/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_X86_RUNPV_PARA_H
#define __ASM_X86_RUNPV_PARA_H

#include <linux/sched.h>
#include <asm/pvm_para.h>

#ifndef __ASSEMBLY__

#define RUNPV_PFN_EVENT_NONE	(0)
#define RUNPV_PFN_EVENT_ALLOC	(1)
#define RUNPV_PFN_EVENT_FREE	(2)

struct page;

#ifdef CONFIG_RUNPV_GUEST
DECLARE_PER_CPU_PAGE_ALIGNED(struct pvm_vcpu_struct, pvm_vcpu_struct);
void runpv_setup_pvcs(int cpu);
int runpv_mark_remap_pvcs_tls(struct task_struct *tsk);
void entry_DIRECTCALL_64_runpv(void);


void runpv_alloc_from_buddy(struct page *pages[], unsigned int orders[], int nr_pages);
void runpv_free_to_buddy(struct page *pages[], unsigned int orders[], int nr_pages);

void runpv_pfn_event_enter(int event);
void runpv_pfn_event_add(int event, struct page *page, unsigned int order);
void runpv_pfn_event_exit(int event);

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

static inline long runpv_hypercall3_retry(long nr, long a0, long a1, long a2)
{
	for (;;) {
		long ret = runpv_hypercall3(nr, a0, a1, a2);
		if (ret == -EAGAIN) {
			continue;
		}
		return ret;
	}
}

void __init runpv_early_setup(unsigned long pgd);

#else

static inline void runpv_setup_pvcs(int cpu) { }

static inline void runpv_early_setup(unsigned long pgd) {

}

static inline void runpv_mark_remap_pvcs_tls(struct task_struct *tsk) {

}


#endif

#ifdef CONFIG_RUNPV_GUEST
void runpv_free_page_hook(struct page *page, unsigned int order);
void runpv_alloc_page_hook(struct page *page, unsigned int order, gfp_t gfp_flags);
#else
static inline void runpv_free_page_hook(struct page *page, unsigned int order)
{

}

static inline void runpv_alloc_page_hook(struct page *page, unsigned int order, gfp_t gfp_flags)
{

}
static inline void runpv_set_pte(unsigned long ptep, unsigned long pte, int level)
{

}
static inline void runpv_alloc_from_buddy(struct page *pages[], unsigned int orders[], int nr_pages) {

}

static inline void runpv_free_to_buddy(struct page *pages[], unsigned int orders[], int nr_pages) {

}

static inline void runpv_pfn_event_enter(int event) {

}

static inline void runpv_pfn_event_add(int event, struct page *page, unsigned int order) {

}

static inline void runpv_pfn_event_exit(int event) {

}

#endif

#endif
#endif
