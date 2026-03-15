#define pr_fmt(fmt) "runpv-guest: " fmt

#include <linux/mm.h>
#include <asm/thread_info.h>
#include <linux/task_work.h>
#include <asm/syscall.h>
#include <asm/runpv_para.h>
#include <linux/mm_types.h>
#include <linux/nospec.h>
#include <linux/sched/debug.h>

#include <asm/cpufeature.h>
#include <asm/cpu_entry_area.h>
#include <asm/desc.h>
#include <asm/pvm_para.h>
#include <asm/setup.h>
#include <asm/traps.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/ktime.h>
#include <asm/tlbflush.h>

void runpv_setup_pvcs(int cpu)
{
	unsigned long kernel_gsbase;
	kernel_gsbase = cpu_kernelmode_gs_base(cpu);
	per_cpu_ptr(&pvm_vcpu_struct, cpu)->kernel_gsbase = kernel_gsbase;
	per_cpu_ptr(&pvm_vcpu_struct, cpu)->yui_entry =
		(unsigned long)entry_DIRECTCALL_64_runpv;
}

static int do_runpv_remap_pvcs_tls(struct task_struct *tsk)
{
	struct vm_area_struct *vma;
	struct mm_struct *mm;
	unsigned long pfn;
	unsigned long uaddr;
	int ret;

	mm = get_task_mm(tsk);

  if(!mm) 
    return 0;

	mmap_write_lock(mm);
  local_irq_disable();
  uaddr = (unsigned long)tsk->pvcs_tls;
	ret = 0;
	vma = find_vma(mm, uaddr);
  pfn = __phys_to_pfn(
      per_cpu_ptr_to_phys(this_cpu_ptr(&pvm_vcpu_struct)));

	if (vma->vm_start == uaddr && vma->vm_end == uaddr + PAGE_SIZE) {
    zap_page_range_single(vma, uaddr, PAGE_SIZE, NULL);
    goto vma_found;
  }

	vma = vm_area_alloc(mm);
	vma->vm_start = uaddr;
	vma->vm_end = uaddr + PAGE_SIZE;
	vm_flags_set(vma, VM_READ | VM_MAYREAD | VM_WRITE | VM_MAYWRITE | VM_DONTEXPAND | VM_MAYSHARE | VM_SHARED | VM_NORESERVE | VM_DONTDUMP);
	vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	BUG_ON(insert_vm_struct(mm, vma) < 0);
  flush_tlb_all();

vma_found:
	ret = remap_pfn_range(vma, uaddr, pfn, PAGE_SIZE, vma->vm_page_prot);
  local_irq_enable();
	mmap_write_unlock(mm);
	mmput(mm);
	BUG_ON(ret);
	return 0;
}

static void runpv_remap_pvcs_tls_work(struct callback_head *head) {
  struct task_struct *tsk = container_of(head, struct task_struct, remap_head);
  do_runpv_remap_pvcs_tls(tsk);
  test_and_clear_tsk_thread_flag(tsk, TIF_PVCS_TLS_REMAP);
}

int runpv_mark_remap_pvcs_tls(struct task_struct * tsk)  {
  if(!test_and_set_tsk_thread_flag(tsk, TIF_PVCS_TLS_REMAP))
    task_work_add(tsk, &tsk->remap_head, TWA_RESUME);
  return 0;
}

SYSCALL_DEFINE1(pvcs_set_tls, unsigned long, tls)
{
	if (!PAGE_ALIGNED(tls)) {
		pr_err("PVCS TLS address 0x%lx is not page aligned\n", tls);
		return -EINVAL;
	}
	current->pvcs_tls = tls; // this is ok for the current task;
	do_runpv_remap_pvcs_tls(current);
  current->remap_head.next = NULL;
  current->remap_head.func = runpv_remap_pvcs_tls_work;
	return 0;
}

__visible noinstr bool do_syscall_64_runpv(struct pt_regs *regs, int nr)
{
	do_syscall_64(regs, nr);
	return true;
}

static inline long runpv_hypercall4(long nr, long a0, long a1, long a2, long a3)
{
	long ret;
	register long r10_reg asm("r10") = a2;
	register long r12_reg asm("r12") = a3;
	asm volatile("call runpv_hypercall"
		     : "=a"(ret), "+D"(nr), "+S"(a0), "+d"(a1), "+r"(r10_reg), "+r"(r12_reg)
		     :
		     : "memory", "rcx", "r8", "r9","r11", "r13", "r14", "rbx", "rbp", "r15");
	if (ret == -ENOSYS) {
		pr_info("NOSYS: nr %lx\n", nr);
	}
	return ret;
}

static inline long runpv_hypercall3(long nr, long a0, long a1, long a2)
{
	return runpv_hypercall4(nr, a0, a1, a2, 0);
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

static inline long runpv_hypercall4_retry(long nr, long a0, long a1, long a2, long a3)
{
	for (;;) {
		long ret = runpv_hypercall4(nr, a0, a1, a2, a3);
		if (ret == -EAGAIN) {
			continue;
		}
		return ret;
	}
}

static inline long runpv_hypercall3_retry(long nr, long a0, long a1, long a2)
{
	return runpv_hypercall4_retry(nr, a0, a1, a2, 0);
}

static inline long runpv_hypercall2_retry(long nr, long a0, long a1)
{
	return runpv_hypercall4_retry(nr, a0, a1, 0, 0);
}

static inline long runpv_hypercall1_retry(long nr, long a0)
{
	return runpv_hypercall4_retry(nr, a0, 0, 0, 0);
}

static inline long runpv_helper_pte_set(unsigned long ptep, unsigned long pte)
{
	return runpv_hypercall2_retry(RUNPV_HC_MMU_PTE_SET, (long)ptep, (long)pte);
}

static inline long runpv_helper_pmd_set(unsigned long pmdp, unsigned long pmd)
{
	return runpv_hypercall2_retry(RUNPV_HC_MMU_PMD_SET, (long)pmdp, (long)pmd);
}

static inline long runpv_helper_pud_set(unsigned long pudp, unsigned long pud)
{
	return runpv_hypercall2_retry(RUNPV_HC_MMU_PUD_SET, (long)pudp, (long)pud);
}

static inline long runpv_helper_p4d_set(unsigned long p4dp, unsigned long p4d)
{
	return runpv_hypercall2_retry(RUNPV_HC_MMU_P4D_SET, (long)p4dp, (long)p4d);
}

static __always_inline unsigned long runpv_gpt2spt_base(void)
{
	static unsigned long gpt2spt_base;
	unsigned long val;

	if (likely(READ_ONCE(gpt2spt_base)))
		return gpt2spt_base;

	val = pvm_hypercall1(PVM_HC_RDMSR, MSR_PVM_GPT2SPT_BASE);
	WRITE_ONCE(gpt2spt_base, val);

	return val;
}

unsigned long runpv_guest_ptep_get(unsigned long *ptep)
{
	unsigned long gpa = __pa(ptep);
	unsigned long gfn, idx, spte, gpte, guest_other_bits, shadow_ad_bits;
	void **gpt2spt;
	void *spt;

	gfn = gpa >> PAGE_SHIFT;
	gpt2spt = (void **)runpv_gpt2spt_base();
	spt = gpt2spt[gfn];

	if (spt == NULL)
		return READ_ONCE(*ptep);

	idx = ((unsigned long)ptep & (PAGE_SIZE - 1)) / sizeof(*ptep);
	do {
		gpte = READ_ONCE(*ptep);
		smp_rmb();
		if (!(gpte & _PAGE_PRESENT))
			return gpte;
		spte = READ_ONCE(((unsigned long *)spt)[idx]);
		smp_rmb();
	} while (unlikely(gpte != READ_ONCE(*ptep)));

	guest_other_bits = gpte & ~(_PAGE_ACCESSED | _PAGE_DIRTY);
	shadow_ad_bits = spte & (_PAGE_ACCESSED | _PAGE_DIRTY);
	return guest_other_bits | shadow_ad_bits;
}

pte_t runpv_ptep_get(pte_t *ptep)
{
	unsigned long guest_read = runpv_guest_ptep_get((unsigned long *)ptep);
	// unsigned long host_read = runpv_hypercall1_retry(RUNPV_HC_MMU_PTEP_GET, (long)ptep);
	// BUG_ON(guest_read != host_read);
	return native_make_pte(guest_read);
}
EXPORT_SYMBOL_GPL(runpv_ptep_get);

pmd_t runpv_pmdp_get(pmd_t *pmdp)
{
	BUG_ON(pmdp->pmd & _PAGE_PSE);
	return READ_ONCE(*pmdp);
	return native_make_pmd(runpv_hypercall1_retry(RUNPV_HC_MMU_PMDP_GET, (long)pmdp));
}
EXPORT_SYMBOL_GPL(runpv_pmdp_get);

pte_t runpv_ptep_get_and_clear(struct mm_struct *mm, unsigned long addr,
			       pte_t *ptep)
{
	return native_make_pte(runpv_hypercall2_retry(RUNPV_HC_MMU_PTEP_GET_AND_CLEAR, (long)__pa(ptep), 0));
	pte_t pte = native_ptep_get_and_clear(ptep);
	return pte;
}
EXPORT_SYMBOL_GPL(runpv_ptep_get_and_clear);

pte_t runpv_ptep_get_and_clear_full(struct mm_struct *mm, unsigned long addr,
				    pte_t *ptep, int full)
{
	pte_t pte;

	if (full) {
		pte = native_make_pte(runpv_hypercall2_retry(RUNPV_HC_MMU_PTEP_GET_AND_CLEAR, (long)__pa(ptep), 1));
		// pte = native_local_ptep_get_and_clear(ptep);
		// page_table_check_pte_clear(mm, pte);
	} else {
		pte = runpv_ptep_get_and_clear(mm, addr, ptep);
	}

	return pte;
}
EXPORT_SYMBOL_GPL(runpv_ptep_get_and_clear_full);

void runpv_ptep_set_wrprotect(struct mm_struct *mm, unsigned long addr,
			      pte_t *ptep)
{
	runpv_hypercall1_retry(RUNPV_HC_MMU_PTEP_SET_WRPROTECT, (long)__pa(ptep));
	return;
	pte_t old_pte, new_pte;

	old_pte = READ_ONCE(*ptep);
	do {
		new_pte = pte_wrprotect(old_pte);
	} while (!try_cmpxchg((long *)&ptep->pte, (long *)&old_pte,
			      *(long *)&new_pte));
}
EXPORT_SYMBOL_GPL(runpv_ptep_set_wrprotect);

int runpv_ptep_set_access_flags(struct vm_area_struct *vma,
				unsigned long address, pte_t *ptep,
				pte_t entry, int dirty)
{
	return (int)runpv_hypercall3_retry(RUNPV_HC_MMU_PTEP_SET_ACCESS_FLAGS, (long)__pa(ptep), (long)entry.pte, dirty);
	int changed = !pte_same(*ptep, entry);

	if (changed && dirty)
		set_pte(ptep, entry);

	return changed;
}
EXPORT_SYMBOL_GPL(runpv_ptep_set_access_flags);

int runpv_ptep_test_and_clear_young(struct vm_area_struct *vma,
				    unsigned long addr, pte_t *ptep)
{
	BUG();
	return (int)runpv_hypercall1_retry(RUNPV_HC_MMU_PTEP_TEST_AND_CLEAR_YOUNG, (long)__pa(ptep));
	int rc = 0;

	if (pte_young(*ptep))
		rc = test_and_clear_bit(_PAGE_BIT_ACCESSED,
					(unsigned long *)&ptep->pte);

	return rc;
}
EXPORT_SYMBOL_GPL(runpv_ptep_test_and_clear_young);

pmd_t runpv_pmdp_huge_get_and_clear(struct mm_struct *mm, unsigned long addr,
				    pmd_t *pmdp)
{
	BUG();
	pmd_t pmd = native_pmdp_get_and_clear(pmdp);

	page_table_check_pmd_clear(mm, pmd);
	return pmd;
}
EXPORT_SYMBOL_GPL(runpv_pmdp_huge_get_and_clear);

pud_t runpv_pudp_huge_get_and_clear(struct mm_struct *mm, unsigned long addr,
				    pud_t *pudp)
{
	BUG();
	pud_t pud = native_pudp_get_and_clear(pudp);

	page_table_check_pud_clear(mm, pud);
	return pud;
}
EXPORT_SYMBOL_GPL(runpv_pudp_huge_get_and_clear);

void runpv_pmdp_set_wrprotect(struct mm_struct *mm, unsigned long addr,
			      pmd_t *pmdp)
{
	pmd_t old_pmd, new_pmd;
	BUG();

	old_pmd = READ_ONCE(*pmdp);
	do {
		new_pmd = pmd_wrprotect(old_pmd);
	} while (!try_cmpxchg((long *)pmdp, (long *)&old_pmd,
			      *(long *)&new_pmd));
}
EXPORT_SYMBOL_GPL(runpv_pmdp_set_wrprotect);

pmd_t runpv_pmdp_establish(struct vm_area_struct *vma, unsigned long address,
			   pmd_t *pmdp, pmd_t pmd)
{
	BUG();
	page_table_check_pmd_set(vma->vm_mm, pmdp, pmd);
	if (IS_ENABLED(CONFIG_SMP))
		return xchg(pmdp, pmd);

	pmd_t old = *pmdp;

	WRITE_ONCE(*pmdp, pmd);
	return old;
}
EXPORT_SYMBOL_GPL(runpv_pmdp_establish);

#if defined(CONFIG_TRANSPARENT_HUGEPAGE)
int runpv_pmdp_set_access_flags(struct vm_area_struct *vma,
				unsigned long address, pmd_t *pmdp,
				pmd_t entry, int dirty)
{
	BUG();
	int changed = !pmd_same(*pmdp, entry);

	VM_BUG_ON(address & ~HPAGE_PMD_MASK);

	if (changed && dirty)
		set_pmd(pmdp, entry);

	return changed;
}
EXPORT_SYMBOL_GPL(runpv_pmdp_set_access_flags);

int runpv_pudp_set_access_flags(struct vm_area_struct *vma,
				unsigned long address, pud_t *pudp,
				pud_t entry, int dirty)
{
	BUG();
	int changed = !pud_same(*pudp, entry);

	VM_BUG_ON(address & ~HPAGE_PUD_MASK);

	if (changed && dirty)
		set_pud(pudp, entry);

	return changed;
}
EXPORT_SYMBOL_GPL(runpv_pudp_set_access_flags);
#endif

#if defined(CONFIG_TRANSPARENT_HUGEPAGE) || \
	defined(CONFIG_ARCH_HAS_NONLEAF_PMD_YOUNG)
int runpv_pmdp_test_and_clear_young(struct vm_area_struct *vma,
				    unsigned long addr, pmd_t *pmdp)
{
	BUG();
	int rc = 0;

	if (pmd_young(*pmdp))
		rc = test_and_clear_bit(_PAGE_BIT_ACCESSED,
					(unsigned long *)pmdp);

	return rc;
}
EXPORT_SYMBOL_GPL(runpv_pmdp_test_and_clear_young);
#endif

#if defined(CONFIG_TRANSPARENT_HUGEPAGE)
int runpv_pudp_test_and_clear_young(struct vm_area_struct *vma,
				    unsigned long addr, pud_t *pudp)
{
	BUG();
	int rc = 0;

	if (pud_young(*pudp))
		rc = test_and_clear_bit(_PAGE_BIT_ACCESSED,
					(unsigned long *)pudp);

	return rc;
}
EXPORT_SYMBOL_GPL(runpv_pudp_test_and_clear_young);

pmd_t runpv_pmdp_invalidate_ad(struct vm_area_struct *vma,
			       unsigned long address, pmd_t *pmdp)
{
	BUG();
	return runpv_pmdp_establish(vma, address, pmdp, pmd_mkinvalid(*pmdp));
}
EXPORT_SYMBOL_GPL(runpv_pmdp_invalidate_ad);
#endif

#define HOST_PAGE_FAULTIN	(1UL << 0)

#define runpv_set_PT_DEBUG 0

void runpv_alloc_page_hook(struct page *page, unsigned int order, gfp_t gfp_flags)
{
  SetPageRunpv(page);
	if (gfp_flags & __GFP_PT) {
		page->host_page = 0xdead;
	}
	return;
}
EXPORT_SYMBOL(runpv_alloc_page_hook);

void runpv_free_page_hook(struct page *page, unsigned int order)
{
	BUG_ON(page->host_page == 0xdead);
	return;
}
EXPORT_SYMBOL(runpv_free_page_hook);

static DEFINE_PER_CPU(struct runpv_batched_pages, runpv_batched_pages) = {
	.event = RUNPV_PFN_EVENT_NONE,
};

void runpv_pfn_event_enter(int event)
{
	struct runpv_batched_pages *batched_pages = get_cpu_ptr(&runpv_batched_pages);
	BUG_ON(batched_pages->event != RUNPV_PFN_EVENT_NONE);
	batched_pages->event = event;
	batched_pages->nr_pages = 0;
}
EXPORT_SYMBOL_GPL(runpv_pfn_event_enter);

void runpv_pfn_event_add(int event, struct page *page, unsigned int order)
{
	struct runpv_batched_pages *batched_pages = this_cpu_ptr(&runpv_batched_pages);
	BUG_ON(batched_pages->event != event);

	switch (event) {
	case RUNPV_PFN_EVENT_ALLOC:
		for (unsigned int i = 0; i < (1 << order); i++) {
			page[i].host_page = HOST_PAGE_FAULTIN;
		}
		break;
	case RUNPV_PFN_EVENT_FREE:
		for (unsigned int i = 0; i < (1 << order); i++) {
			if (!(page[i].host_page & HOST_PAGE_FAULTIN))
				return;
			page[i].host_page = 0;
		}
		break;
	default:
		BUG();
	}

	batched_pages->pages[batched_pages->nr_pages] = page;
	batched_pages->gfns[batched_pages->nr_pages] = page_to_pfn(page);
	batched_pages->orders[batched_pages->nr_pages] = order;
	batched_pages->nr_pages++;
	BUG_ON(batched_pages->nr_pages >= RUNPV_BATCHED_PAGES_MAX_NR);
}
EXPORT_SYMBOL_GPL(runpv_pfn_event_add);

void runpv_pfn_event_exit(int event)
{
	struct runpv_batched_pages *batched_pages = this_cpu_ptr(&runpv_batched_pages);
	BUG_ON(batched_pages->event != event);
	if (batched_pages->nr_pages > 0) {
		switch (event) {
		case RUNPV_PFN_EVENT_ALLOC:
			pvm_hypercall3(PVM_HC_ALLOC_FROM_BUDDY, (long)batched_pages->gfns, (long)batched_pages->orders, batched_pages->nr_pages);
			break;
		case RUNPV_PFN_EVENT_FREE:
			pvm_hypercall3(PVM_HC_FREE_TO_BUDDY, (long)batched_pages->gfns, (long)batched_pages->orders, batched_pages->nr_pages);
			break;
		default:
			BUG();
		}
	}
	batched_pages->event = RUNPV_PFN_EVENT_NONE;
	put_cpu_ptr(&runpv_batched_pages);
}
EXPORT_SYMBOL_GPL(runpv_pfn_event_exit);

static void runpv_set_pte(pte_t *ptep, pte_t pteval)
{
	// pr_info("%s parent %#lx pfn %#lx\n", __func__, __pa(ptep), (native_pte_val(pteval) & PHYSICAL_PAGE_MASK) >> PAGE_SHIFT);
	if (runpv_helper_pte_set(__pa(ptep), native_pte_val(pteval))) {
		// pr_info("set pte %#lx to ptep %#lx failed\n", native_pte_val(pteval), ptep);
		native_set_pte(ptep, pteval);
	}
}

static void runpv_set_pmd(pmd_t *pmdp, pmd_t pmdval)
{
	// pr_info("%s parent %#lx pfn %#lx\n", __func__, __pa(pmdp), (native_pmd_val(pmdval) & PHYSICAL_PAGE_MASK) >> PAGE_SHIFT);
	if (runpv_helper_pmd_set(__pa(pmdp), native_pmd_val(pmdval))) {
		// pr_info("set pmd %#lx to pmdp %#lx failed\n", native_pmd_val(pmdval), pmdp);
		native_set_pmd(pmdp, pmdval);
	}
}

static void runpv_set_pud(pud_t *pudp, pud_t pudval)
{
	// pr_info("%s parent %#lx pfn %#lx\n", __func__, __pa(pudp), (native_pud_val(pudval) & PHYSICAL_PAGE_MASK) >> PAGE_SHIFT);
	if (runpv_helper_pud_set(__pa(pudp), native_pud_val(pudval))) {
		// pr_info("set pud %#lx to pudp %#lx failed\n", native_pud_val(pudval), pudp);
		native_set_pud(pudp, pudval);
	}
}

static void runpv_set_p4d(p4d_t *p4dp, p4d_t p4dval)
{
	// pr_info("%s parent %#lx pfn %#lx\n", __func__, __pa(p4dp), (native_p4d_val(p4dval) & PHYSICAL_PAGE_MASK) >> PAGE_SHIFT);
	if (runpv_helper_p4d_set(__pa(p4dp), native_p4d_val(p4dval))) {
		// pr_info("set p4d %#lx to p4dp %#lx failed\n", native_p4d_val(p4dval), p4dp);
		native_set_p4d(p4dp, p4dval);
	}
}

static void runpv_alloc_pte(struct mm_struct *mm, unsigned long pfn)
{
	pvm_hypercall2(PVM_HC_ALLOC_PTE, pfn, PVM_SET_PTE_PTE);
}

static void runpv_alloc_pmd(struct mm_struct *mm, unsigned long pfn)
{
	pvm_hypercall2(PVM_HC_ALLOC_PTE, pfn, PVM_SET_PTE_PMD);
}

static void runpv_alloc_pud(struct mm_struct *mm, unsigned long pfn)
{
	pvm_hypercall2(PVM_HC_ALLOC_PTE, pfn, PVM_SET_PTE_PUD);
}

static void runpv_alloc_p4d(struct mm_struct *mm, unsigned long pfn)
{
	BUG();
	pvm_hypercall2(PVM_HC_ALLOC_PTE, pfn, PVM_SET_PTE_P4D);
}

static int runpv_pgd_alloc(struct mm_struct *mm)
{
	pvm_hypercall2(PVM_HC_ALLOC_PTE, __pa(mm->pgd) >> PAGE_SHIFT, PVM_SET_PTE_P4D);
	return 0;
}

static void runpv_release_pte(unsigned long pfn)
{
	pvm_hypercall2(PVM_HC_RELEASE_PTE, pfn, PVM_SET_PTE_PTE);
}

static void runpv_release_pmd(unsigned long pfn)
{
	pvm_hypercall2(PVM_HC_RELEASE_PTE, pfn, PVM_SET_PTE_PMD);
}

static void runpv_release_pud(unsigned long pfn)
{
	pvm_hypercall2(PVM_HC_RELEASE_PTE, pfn, PVM_SET_PTE_PUD);
}

static void runpv_release_p4d(unsigned long pfn)
{
	BUG();
	pvm_hypercall2(PVM_HC_RELEASE_PTE, pfn, PVM_SET_PTE_P4D);
}

static void runpv_pgd_free(struct mm_struct *mm, pgd_t *pgd)
{
	pvm_hypercall2(PVM_HC_RELEASE_PTE, __pa(mm->pgd) >> PAGE_SHIFT, PVM_SET_PTE_P4D);
}

static void runpv_flush_tlb_user(void)
{
	pvm_hypercall0(PVM_HC_TLB_FLUSH_CURRENT);
}

void __init runpv_init_direct_mapping_shadow(unsigned long start_pfn, unsigned long end_pfn)
{
	// int ret;
	// ret = pvm_hypercall2(PVM_HC_INIT_DIRECT_MAPPING_SHADOW, start_pfn, end_pfn);
	// BUG_ON(ret < 0);
}

static void runpv_flush_tlb_kernel(void)
{
	pvm_hypercall0(PVM_HC_TLB_FLUSH);
}

static void runpv_flush_tlb_one_user(unsigned long addr)
{
	pvm_hypercall1(PVM_HC_TLB_INVLPG, addr);
}

void __init runpv_early_setup(unsigned long pgd)
{
	pvm_hypercall2(PVM_HC_ALLOC_PTE, __pa(pgd) >> PAGE_SHIFT, PVM_SET_PTE_P4D);

	pv_ops.mmu.set_pte = runpv_set_pte;
	pv_ops.mmu.set_pmd = runpv_set_pmd;
	pv_ops.mmu.set_pud = runpv_set_pud;
	pv_ops.mmu.set_p4d = runpv_set_p4d;

	pv_ops.mmu.alloc_pte = runpv_alloc_pte;
	pv_ops.mmu.alloc_pmd = runpv_alloc_pmd;
	pv_ops.mmu.alloc_pud = runpv_alloc_pud;
	pv_ops.mmu.alloc_p4d = runpv_alloc_p4d;
	pv_ops.mmu.release_pte = runpv_release_pte;
	pv_ops.mmu.release_pmd = runpv_release_pmd;
	pv_ops.mmu.release_pud = runpv_release_pud;
	pv_ops.mmu.release_p4d = runpv_release_p4d;
	pv_ops.mmu.pgd_alloc = runpv_pgd_alloc;
	pv_ops.mmu.pgd_free = runpv_pgd_free;

	pv_ops.mmu.flush_tlb_user = runpv_flush_tlb_user;
	pv_ops.mmu.flush_tlb_kernel = runpv_flush_tlb_kernel;
	pv_ops.mmu.flush_tlb_one_user = runpv_flush_tlb_one_user;
}

void manual_bug(void) {
  panic("Unexpected fallthrough");
}
