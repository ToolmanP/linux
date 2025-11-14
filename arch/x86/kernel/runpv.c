#define pr_fmt(fmt) "runpv-guest: " fmt

#include <linux/mm.h>
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

void runpv_setup_pvcs(int cpu)
{
	unsigned long kernel_gsbase;
	kernel_gsbase = cpu_kernelmode_gs_base(cpu);
	per_cpu_ptr(&pvm_vcpu_struct, cpu)->kernel_gsbase = kernel_gsbase;
	per_cpu_ptr(&pvm_vcpu_struct, cpu)->yui_entry =
		(unsigned long)entry_DIRECTCALL_64_runpv;
}

int runpv_remap_pvcs_tls(struct task_struct *p, int dest_cpu)
{
	struct vm_area_struct *vma;
	struct mm_struct *mm;
	unsigned long pfn;
	unsigned long uaddr;
	int ret;

	pfn = __phys_to_pfn(
		per_cpu_ptr_to_phys(per_cpu_ptr(&pvm_vcpu_struct, dest_cpu)));
	uaddr = (unsigned long)p->pvcs_tls;
	ret = 0;

	mm = get_task_mm(p);
	mmap_write_lock(mm);

	vma = find_vma(mm, uaddr);

	if (!vma) {
		pr_err("PVCS TLS vma not found for task %s\n", p->comm);
		ret = -ENOENT;
		goto out_unlock;
	}

	ret = do_munmap(mm, uaddr, PAGE_SIZE, NULL);

	if (ret) {
		pr_err("Failed to unmap PVCS TLS vma for task %s: %d\n",
		       p->comm, ret);
		goto out_unlock;
	}

	vma = vm_area_alloc(mm);
	vma->vm_start = uaddr;
	vma->vm_end = uaddr + PAGE_SIZE;
	vm_flags_set(vma, VM_READ | VM_WRITE | VM_MAYREAD | VM_MAYWRITE |
				  VM_SHARED | VM_DONTEXPAND);
	vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	BUG_ON(insert_vm_struct(mm, vma) < 0);
	BUG_ON(vm_insert_page(vma, uaddr, pfn_to_page(pfn)) < 0);
	flush_tlb_one_user(uaddr);
out_unlock:
	mmap_write_unlock(mm);
	mmput(mm);
	BUG_ON(ret);
	return 0;
}

SYSCALL_DEFINE1(pvcs_set_tls, unsigned long, tls)
{
	if (!PAGE_ALIGNED(tls)) {
		pr_err("PVCS TLS address 0x%lx is not page aligned\n", tls);
		return -EINVAL;
	}
	current->pvcs_tls = tls; // this is ok for the current task;
	local_irq_disable(); // we have to make sure that cpu is not changed while we are remapping the TLS
	runpv_remap_pvcs_tls(current, smp_processor_id());
	local_irq_enable();
	return 0;
}

__visible noinstr bool do_syscall_64_runpv(struct pt_regs *regs, int nr)
{
	do_syscall_64(regs, nr);
	return true;
}

#ifdef CONFIG_RUNPV_MEM_PARAVIRT
#define HOST_PAGE_NONE		(0)
#define HOST_PAGE_NORMAL	(1)
#define HOST_PAGE_PT		(2)

#define runpv_set_PT_DEBUG 0
static inline int pte_is_host_page_normal(pte_t pte)
{
	if (pte.pte & _PAGE_PRESENT) {
		struct page *page = pte_page(pte);
		return page->host_page == HOST_PAGE_NORMAL;
	}
	return 1;
}

static inline int runpv_hc_set_pte(pte_t *ptep, pte_t pte)
{
	for (;;) {
		long ret = runpv_hypercall2(RUNPV_HC_SET_PTE, (long)ptep, (long)pte.pte);
		if (ret == -EAGAIN) {
			continue;
		} else {
			return (int)ret;
		}
	}
}

static inline int runpv_hc_bind_host_page(long gfn, int order, unsigned long page)
{
	for (;;) {
		long ret = runpv_hypercall3(RUNPV_HC_BIND_HOST_PAGE, gfn, order, page);
		if (ret == -EAGAIN) {
			continue;
		} else if (ret == -ENODATA) {
			pvm_hypercall0(PVM_HC_SYNC_PAGES);
		} else {
			return (int)ret;
		}
	}
}

static inline int runpv_hc_unbind_host_page(long gfn, unsigned long page)
{
	for (;;) {
		long ret = runpv_hypercall2(RUNPV_HC_UNBIND_HOST_PAGE, gfn, page);
		if (ret == -EAGAIN) {
			continue;
		} else if (ret == -ENODATA) {
			pvm_hypercall0(PVM_HC_SYNC_FREE_PAGES);
		} else {
			return (int)ret;
		}
	}
}

static inline int runpv_hc_mark_page_pt(long gfn, unsigned long page, int mark)
{
	long ret;
	for (;;) {
		ret = runpv_hypercall3(RUNPV_HC_MARK_PAGE_PT, gfn, page, mark);
		if (ret == -EAGAIN) {
			continue;
		} else {
			return (int)ret;
		}
	}
}

static int split_vmm_enable = 0;

SYSCALL_DEFINE0(split_vmm_enable)
{
	split_vmm_enable = 1;
	return 0;
}


void runpv_alloc_page_hook(struct page *page, unsigned int order, gfp_t gfp_flags)
{
	int ret;
	if (order) {
		return;
	}
	BUG_ON(page->host_page != HOST_PAGE_NONE);
	if (gfp_flags & __GFP_PT) {
		ret = runpv_hc_mark_page_pt(page_to_pfn(page), (unsigned long)page_to_virt(page), 1);
		page->host_page = HOST_PAGE_PT;
	} else if (split_vmm_enable) {
		ret = runpv_hc_bind_host_page(page_to_pfn(page), order, (long) page_to_virt(page));
		if (ret == 0) {
			page->host_page = HOST_PAGE_NORMAL;
		}
	}
}

EXPORT_SYMBOL(runpv_alloc_page_hook);

void runpv_free_page_hook(struct page *page, unsigned int order)
{
	int ret;
	if (page->host_page == HOST_PAGE_PT) {
		ret = runpv_hc_mark_page_pt(page_to_pfn(page), (unsigned long)page_to_virt(page), 0);
		page->host_page = HOST_PAGE_NONE;
	} else if (page->host_page == HOST_PAGE_NORMAL) {
		ret = runpv_hc_unbind_host_page(page_to_pfn(page), (unsigned long)page_to_virt(page));
		page->host_page = HOST_PAGE_NONE;
	}
}
EXPORT_SYMBOL(runpv_free_page_hook);


static int runpv_try_set_pte(pte_t *pte, pte_t entry)
{
	int ret;
	if (!split_vmm_enable) return -1;
	if (!pte_is_host_page_normal(*pte)) return -1;
	if (!pte_is_host_page_normal(entry)) return -1;
	ret = runpv_hc_set_pte(pte, entry);
	if (ret == 0) {
		BUG_ON(pte->pte != entry.pte);
	}
	return ret;
}


static void runpv_set_pte(pte_t *ptep, pte_t pteval)
{
	if (runpv_try_set_pte(ptep, pteval)) {
		native_set_pte(ptep, pteval);
	}
}

static void runpv_set_pmd(pmd_t *pmdp, pmd_t pmdval)
{
	native_set_pmd(pmdp, pmdval);
}

static void runpv_set_pud(pud_t *pudp, pud_t pudval)
{
	native_set_pud(pudp, pudval);
}

static void runpv_set_p4d(p4d_t *p4dp, p4d_t p4dval)
{
	native_set_p4d(p4dp, p4dval);
}

#endif

static void runpv_flush_tlb_user(void)
{
#ifdef CONFIG_RUNPV_MEM_PARAVIRT
	runpv_hypercall0(RUNPV_HC_TLB_FLUSH_CURRENT);
#endif
	pvm_hypercall0(PVM_HC_TLB_FLUSH_CURRENT);
}

static void runpv_flush_tlb_kernel(void)
{
#ifdef CONFIG_RUNPV_MEM_PARAVIRT
	runpv_hypercall0(RUNPV_HC_TLB_FLUSH);
#endif
	pvm_hypercall0(PVM_HC_TLB_FLUSH);
}

static void runpv_flush_tlb_one_user(unsigned long addr)
{
#ifdef CONFIG_RUNPV_MEM_PARAVIRT
	runpv_hypercall1(RUNPV_HC_TLB_INVLPG, addr);
#endif
	pvm_hypercall1(PVM_HC_TLB_INVLPG, addr);
}

void __init runpv_early_setup(void){
#ifdef CONFIG_RUNPV_MEM_PARAVIRT
	pv_ops.mmu.set_pte = runpv_set_pte;
	pv_ops.mmu.set_pmd = runpv_set_pmd;
	pv_ops.mmu.set_pud = runpv_set_pud;
	pv_ops.mmu.set_p4d = runpv_set_p4d;
#endif
	pv_ops.mmu.flush_tlb_user = runpv_flush_tlb_user;
	pv_ops.mmu.flush_tlb_kernel = runpv_flush_tlb_kernel;
	pv_ops.mmu.flush_tlb_one_user = runpv_flush_tlb_one_user;
}
