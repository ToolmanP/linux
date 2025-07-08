#define pr_fmt(fmt) "yui-guest: " fmt

#include <asm/syscall.h>
#include <asm/yui_para.h>
#include <linux/mm_types.h>
#include <linux/nospec.h>

#include <asm/cpufeature.h>
#include <asm/cpu_entry_area.h>
#include <asm/desc.h>
#include <asm/pvm_para.h>
#include <asm/setup.h>
#include <asm/traps.h>

void yui_setup_pvcs(int cpu)
{
	unsigned long kernel_gsbase;
	kernel_gsbase = cpu_kernelmode_gs_base(cpu);
	per_cpu_ptr(&pvm_vcpu_struct, cpu)->kernel_gsbase = kernel_gsbase;
	per_cpu_ptr(&pvm_vcpu_struct, cpu)->yui_addr = (unsigned long)entry_DIRECTCALL_64_yui;
	per_cpu_ptr(&pvm_vcpu_struct, cpu)->reserved1 = 1;
}

int yui_remap_pvcs_tls(struct task_struct *p, int dest_cpu)
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
	migrate_disable(); // we have to make sure that cpu is not changed while we are remapping the TLS
	yui_remap_pvcs_tls(current, smp_processor_id());
	migrate_enable();
	return 0;
}

SYSCALL_DEFINE0(yui_direct){
	return (long)entry_DIRECTCALL_64_yui;
}

__visible noinstr bool do_syscall_64_yui(struct pt_regs *regs, int nr){
	return do_syscall_64(regs, nr);
}
