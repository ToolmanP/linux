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

#ifdef CONFIG_RUNPV_MEM_PARAVIRT

static int runpv_set_pte(void *ptep, phys_addr_t pte, int level)
{
  return pvm_hypercall3(PVM_HC_MMU_SET_PTE, __pa((unsigned long)ptep), (unsigned long)pte, (unsigned long) level);
}

void runpv_alloc_page_hook(struct page *page, unsigned int order)
{
  SetPageRunpv(page);
  pvm_hypercall2(PVM_HC_MARK_KPFN, (unsigned long)page_to_pfn(page), (unsigned long)order);
}
EXPORT_SYMBOL(runpv_alloc_page_hook);

void runpv_free_page_hook(struct page *page, unsigned int order)
{
  pvm_hypercall2(PVM_HC_FREE_KPFN, (unsigned long)page_to_pfn(page), (unsigned long)order);
}
EXPORT_SYMBOL(runpv_free_page_hook);

static void runpv_set_pmd(pmd_t *pmdp, pmd_t pmdval)
{
    native_set_pmd(pmdp, pmdval);
}

static void runpv_set_pud(pud_t *pudp, pud_t pudval)
{
    native_set_pud(pudp, pudval);
}

#endif

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

void __init runpv_early_setup(void){
#ifdef CONFIG_RUNPV_MEM_PARAVIRT
	pv_ops.mmu.set_pmd = runpv_set_pmd;
	pv_ops.mmu.set_pud = runpv_set_pud;
#endif
	pv_ops.mmu.flush_tlb_user = runpv_flush_tlb_user;
	pv_ops.mmu.flush_tlb_kernel = runpv_flush_tlb_kernel;
	pv_ops.mmu.flush_tlb_one_user = runpv_flush_tlb_one_user;
}

void manual_bug(void) {
  panic("Unexpected fallthrough");
}
