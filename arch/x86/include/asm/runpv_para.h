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
pte_t runpv_ptep_get_and_clear(struct mm_struct *mm, unsigned long addr,
			       pte_t *ptep);
pte_t runpv_ptep_get_and_clear_full(struct mm_struct *mm, unsigned long addr,
				    pte_t *ptep, int full);
void runpv_ptep_set_wrprotect(struct mm_struct *mm, unsigned long addr,
			      pte_t *ptep);
int runpv_ptep_set_access_flags(struct vm_area_struct *vma,
				unsigned long address, pte_t *ptep,
				pte_t entry, int dirty);
int runpv_ptep_test_and_clear_young(struct vm_area_struct *vma,
				    unsigned long addr, pte_t *ptep);
pmd_t runpv_pmdp_huge_get_and_clear(struct mm_struct *mm, unsigned long addr,
				    pmd_t *pmdp);
pud_t runpv_pudp_huge_get_and_clear(struct mm_struct *mm, unsigned long addr,
				    pud_t *pudp);
void runpv_pmdp_set_wrprotect(struct mm_struct *mm, unsigned long addr,
			      pmd_t *pmdp);
pmd_t runpv_pmdp_establish(struct vm_area_struct *vma, unsigned long address,
			   pmd_t *pmdp, pmd_t pmd);
#if defined(CONFIG_TRANSPARENT_HUGEPAGE)
int runpv_pmdp_set_access_flags(struct vm_area_struct *vma,
				unsigned long address, pmd_t *pmdp,
				pmd_t entry, int dirty);
int runpv_pudp_set_access_flags(struct vm_area_struct *vma,
				unsigned long address, pud_t *pudp,
				pud_t entry, int dirty);
int runpv_pudp_test_and_clear_young(struct vm_area_struct *vma,
				    unsigned long addr, pud_t *pudp);
pmd_t runpv_pmdp_invalidate_ad(struct vm_area_struct *vma,
			       unsigned long address, pmd_t *pmdp);
#endif
#if defined(CONFIG_TRANSPARENT_HUGEPAGE) || \
	defined(CONFIG_ARCH_HAS_NONLEAF_PMD_YOUNG)
int runpv_pmdp_test_and_clear_young(struct vm_area_struct *vma,
				    unsigned long addr, pmd_t *pmdp);
#endif

#define runpv_bug_stub(...) ({ BUG(); __builtin_unreachable(); })

#define ptep_get runpv_ptep_get
#define pmdp_get runpv_pmdp_get
#define ptep_get_and_clear runpv_ptep_get_and_clear
#define ptep_get_and_clear_full runpv_ptep_get_and_clear_full
#define ptep_set_wrprotect runpv_ptep_set_wrprotect
#define ptep_set_access_flags runpv_ptep_set_access_flags
#define ptep_test_and_clear_young runpv_ptep_test_and_clear_young
#define pmdp_huge_get_and_clear runpv_bug_stub
#define pudp_huge_get_and_clear runpv_bug_stub
#define pmdp_set_wrprotect runpv_pmdp_set_wrprotect
#define pmdp_establish runpv_pmdp_establish
#if defined(CONFIG_TRANSPARENT_HUGEPAGE)
#define pmdp_set_access_flags runpv_pmdp_set_access_flags
#define pudp_set_access_flags runpv_pudp_set_access_flags
#define pudp_test_and_clear_young runpv_pudp_test_and_clear_young
#define pmdp_invalidate_ad runpv_bug_stub
#endif
#if defined(CONFIG_TRANSPARENT_HUGEPAGE) || \
	defined(CONFIG_ARCH_HAS_NONLEAF_PMD_YOUNG)
#define pmdp_test_and_clear_young runpv_pmdp_test_and_clear_young
#endif

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
