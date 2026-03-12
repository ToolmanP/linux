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

#define HOST_PAGE_FAULTIN	(1UL << 0)

#define runpv_set_PT_DEBUG 0

void runpv_alloc_page_hook(struct page *page, unsigned int order, gfp_t gfp_flags)
{
  SetPageRunpv(page);
//   pvm_hypercall2(PVM_HC_MARK_KPFN, (unsigned long)page_to_pfn(page), (unsigned long)order);
	if (gfp_flags & __GFP_PT) {
		page->host_page = 0xdead;
	}
	return;
}
EXPORT_SYMBOL(runpv_alloc_page_hook);

void runpv_free_page_hook(struct page *page, unsigned int order)
{
//   pvm_hypercall2(PVM_HC_FREE_KPFN, (unsigned long)page_to_pfn(page), (unsigned long)order);
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
			// pr_info("%s: alloc %u pages from buddy\n", __func__, batched_pages->nr_pages);
			pvm_hypercall3(PVM_HC_ALLOC_FROM_BUDDY, batched_pages->gfns, batched_pages->orders, batched_pages->nr_pages);
			break;
		case RUNPV_PFN_EVENT_FREE:
			// pr_info("%s: free %u pages to buddy\n", __func__, batched_pages->nr_pages);
			pvm_hypercall3(PVM_HC_FREE_TO_BUDDY, batched_pages->gfns, batched_pages->orders, batched_pages->nr_pages);
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
	if (runpv_hypercall3_retry(RUNPV_HC_SET_PTE, (long)__pa(ptep), (long)native_pte_val(pteval), PVM_SET_PTE_PTE)) {
		// pr_info("set pte %#lx to ptep %#lx failed\n", native_pte_val(pteval), ptep);
		native_set_pte(ptep, pteval);
	}
}

static void runpv_set_pmd(pmd_t *pmdp, pmd_t pmdval)
{
	// pr_info("%s parent %#lx pfn %#lx\n", __func__, __pa(pmdp), (native_pmd_val(pmdval) & PHYSICAL_PAGE_MASK) >> PAGE_SHIFT);
	if (runpv_hypercall3_retry(RUNPV_HC_SET_PTE, (long)__pa(pmdp), (long)native_pmd_val(pmdval), PVM_SET_PTE_PMD)) {
		// pr_info("set pmd %#lx to pmdp %#lx failed\n", native_pmd_val(pmdval), pmdp);
		native_set_pmd(pmdp, pmdval);
	}
}

static void runpv_set_pud(pud_t *pudp, pud_t pudval)
{
	// pr_info("%s parent %#lx pfn %#lx\n", __func__, __pa(pudp), (native_pud_val(pudval) & PHYSICAL_PAGE_MASK) >> PAGE_SHIFT);
	if (runpv_hypercall3_retry(RUNPV_HC_SET_PTE, (long)__pa(pudp), (long)native_pud_val(pudval), PVM_SET_PTE_PUD)) {
		// pr_info("set pud %#lx to pudp %#lx failed\n", native_pud_val(pudval), pudp);
		native_set_pud(pudp, pudval);
	}
}

static void runpv_set_p4d(p4d_t *p4dp, p4d_t p4dval)
{
	// pr_info("%s parent %#lx pfn %#lx\n", __func__, __pa(p4dp), (native_p4d_val(p4dval) & PHYSICAL_PAGE_MASK) >> PAGE_SHIFT);
	if (runpv_hypercall3_retry(RUNPV_HC_SET_PTE, (long)__pa(p4dp), (long)native_p4d_val(p4dval), PVM_SET_PTE_P4D)) {
		// pr_info("set p4d %#lx to p4dp %#lx failed\n", native_p4d_val(p4dval), p4dp);
		native_set_p4d(p4dp, p4dval);
	}
}

static void runpv_alloc_pte(struct mm_struct *mm, unsigned long pfn)
{
	// pr_info("%s pfn %#lx\n", __func__, pfn);
	pvm_hypercall2(PVM_HC_ALLOC_PTE, pfn, PVM_SET_PTE_PTE);
}

static void runpv_alloc_pmd(struct mm_struct *mm, unsigned long pfn)
{
	// pr_info("%s pfn %#lx\n", __func__, pfn);
	pvm_hypercall2(PVM_HC_ALLOC_PTE, pfn, PVM_SET_PTE_PMD);
}

static void runpv_alloc_pud(struct mm_struct *mm, unsigned long pfn)
{
	// pr_info("%s pfn %#lx\n", __func__, pfn);
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
