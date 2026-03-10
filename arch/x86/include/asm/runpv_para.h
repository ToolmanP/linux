/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_X86_RUNPV_PARA_H
#define __ASM_X86_RUNPV_PARA_H

#include <asm/pvm_para.h>

#ifndef __ASSEMBLY__

#include <linux/sched.h>

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

pte_t runpv_ptep_get(pte_t *ptep);
pmd_t runpv_pmdp_get(pmd_t *pmdp);

#define ptep_get runpv_ptep_get
#define pmdp_get runpv_pmdp_get

void __init runpv_early_setup(unsigned long pgd);
void __init runpv_init_direct_mapping_shadow(unsigned long start_pfn, unsigned long end_pfn);

#else

static inline void runpv_setup_pvcs(int cpu) { }

static inline void runpv_early_setup(unsigned long pgd) {

}

static inline void runpv_init_direct_mapping_shadow(unsigned long start_pfn, unsigned long end_pfn) {

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
