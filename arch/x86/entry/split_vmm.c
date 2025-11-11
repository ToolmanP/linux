// SPDX-License-Identifier: GPL-2.0-only
/*
 * common.c - C code for kernel entry and exit
 * Copyright (c) 2015 Andrew Lutomirski
 *
 * Based on asm and ptrace code by many authors.  The code here originated
 * in ptrace.c and signal.c.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/linkage.h>
#include <uapi/asm/pvm_para.h>
#include <linux/kvm_host.h>

#include <asm/desc.h>
#include <asm/mem_encrypt.h>
#include <asm/pgtable_types.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/special_insns.h>
#include <asm/processor-flags.h>
#include "split_vmm.h"

#define SHADOW_PRESENT (1 << 11)
#define check_shadow_present(pte) (pte_val(pte) & SHADOW_PRESENT)

#define RUNPV_HC_DEBUG 0

static inline long runpv_mmu_write_trylock(void)
{
	unsigned long mmu_lock_ptr = this_cpu_read(cpu_tss_rw.tss_ex.mmu_lock_ptr);
	if (!mmu_lock_ptr)
		return -EINVAL;
	if (do_raw_write_trylock((rwlock_t *)mmu_lock_ptr))
		return 0;
	return -EAGAIN;
}

static inline void runpv_mmu_write_unlock(void)
{
	unsigned long mmu_lock_ptr = this_cpu_read(cpu_tss_rw.tss_ex.mmu_lock_ptr);
	if (!mmu_lock_ptr)
		return;
	do_raw_write_unlock((rwlock_t *)mmu_lock_ptr);
}

long runpv_hc_handle_set_pte(long ptep, long pte) {
	unsigned long *entry_ptr;
	int actual_level;
	int ret;
	unsigned long ptep_pa;
	unsigned long kernel_cr3 = this_cpu_read(cpu_tss_rw.tss_ex.smod_cr3);
	unsigned long user_cr3 = this_cpu_read(cpu_tss_rw.tss_ex.umod_cr3);
	unsigned long current_cr3 = this_cpu_read(cpu_tss_rw.tss_ex.fast_hypercall_current_cr3);
	pv_info(RUNPV_HC_DEBUG, "ptep: %#lx, pte: %#lx\n", ptep, pte);
	pv_info(RUNPV_HC_DEBUG, "current_cr3: %#lx, kernel_cr3: %#lx, user_cr3: %#lx\n", current_cr3, kernel_cr3, user_cr3);

	ret = runpv_mmu_write_trylock();
	if (ret < 0) {
		return ret;
	}

	struct runpv_page_array *page_array = (struct runpv_page_array *)this_cpu_read(cpu_tss_rw.tss_ex.page_array_base);
	unsigned long gfn = (pte & PHYSICAL_PAGE_MASK) >> PAGE_SHIFT;

	ret = va_to_pa(kernel_cr3, ptep, &ptep_pa);
	if (ret < 0) {
		pr_err("va_to_pa failed: %d\n", ret);
		goto out_unlock;
	}

	unsigned long *shadow_pfns;
	unsigned long page_offset_base = this_cpu_read(cpu_tss_rw.tss_ex.page_offset_base);
	unsigned long ptep_gfn = ((ptep - page_offset_base) & PHYSICAL_PAGE_MASK) >> PAGE_SHIFT;
	runpv_get_shadow_pfn(page_array, ptep_gfn, &shadow_pfns);

	unsigned long pfn;
	runpv_get_host_page(page_array, gfn, &pfn);

	unsigned long shadow_pte = ((pfn << PAGE_SHIFT) & PHYSICAL_PAGE_MASK) | _PAGE_DIRTY | (pte & ~PHYSICAL_PAGE_MASK);
	for (int i = 0; i < RUNPV_PAGE_ARRAY_SHADOW_COUNT; i++) {
		unsigned long shadow_pfn = shadow_pfns[i];
		if (!shadow_pfn) continue;
		unsigned long *test_entry_ptr = (unsigned long *)phys_to_virt((shadow_pfn << PAGE_SHIFT) | (ptep & (PAGE_SIZE-1)));
		// pv_info(RUNPV_HC_DEBUG, "set shadow pte %#lx test_entry_ptr %#lx\n", shadow_pte, test_entry_ptr);
		WRITE_ONCE(*(pte_t *)test_entry_ptr, __pte(shadow_pte));
	}
	// pv_info(RUNPV_HC_DEBUG, "set pte %#lx to pfn %#lx\n", ptep, pfn);
	WRITE_ONCE(*(pte_t *)phys_to_virt(ptep_pa), __pte(pte));

	ret = 0;

out_unlock:
	runpv_mmu_write_unlock();
	return ret;
}

long runpv_hc_handle_bind_host_page(unsigned long gfn, int order, unsigned long kva)
{
	struct runpv_page_array *page_array = (struct runpv_page_array *)this_cpu_read(cpu_tss_rw.tss_ex.page_array_base);
	struct runpv_page_buffer *page_buffer = (struct runpv_page_buffer *)this_cpu_read(cpu_tss_rw.tss_ex.page_buffer_base);
	unsigned long pfn;
	unsigned long kernel_cr3;
	unsigned long *entry_ptr;
	int actual_level;
	int ret;

	ret = runpv_mmu_write_trylock();
	if (ret < 0) {
		return ret;
	}

	kernel_cr3 = this_cpu_read(cpu_tss_rw.tss_ex.smod_cr3);
	ret = pw_walk_address(kva, kernel_cr3, PW_LEVEL_PTE, &entry_ptr, &actual_level);
	if (ret < 0 || actual_level != PW_LEVEL_PTE) {
		pv_info(RUNPV_HC_DEBUG, "%s: pw_walk_address fail %d kva %#lx\n", __func__, ret, kva);
		goto out_unlock;
	}
	if (check_shadow_present(*(pte_t *)entry_ptr)) {
		pv_info(RUNPV_HC_DEBUG, "%s: pte %#lx is already shadowed kva %#lx\n", __func__, entry_ptr, kva);
		ret = -EINVAL;
		goto out_unlock;
	}

	ret = get_one_page(page_buffer, &pfn);
	if (ret < 0) {
		pv_info(RUNPV_HC_DEBUG, "%s: get_one_page fail %d\n", __func__, ret);
		goto out_unlock;
	}

	runpv_bind_host_page(page_array, gfn, pfn);

	BUG_ON(check_shadow_present(*(pte_t *)entry_ptr));
	pv_info(RUNPV_HC_DEBUG, "PVM: set pte %#lx from %#lx to pfn %#lx kva %#lx\n", entry_ptr, *(pte_t *)entry_ptr, pfn, kva);
	WRITE_ONCE(*(pte_t *)entry_ptr, pfn_pte(pfn, PAGE_KERNEL_PVM));

out_unlock:
	runpv_mmu_write_unlock();
	return ret;
}

long runpv_hc_handle_unbind_host_page(unsigned long gfn, unsigned long kva)
{
	struct runpv_page_array *page_array = (struct runpv_page_array *)this_cpu_read(cpu_tss_rw.tss_ex.page_array_base);
	struct runpv_page_buffer *page_buffer = (struct runpv_page_buffer *)this_cpu_read(cpu_tss_rw.tss_ex.page_buffer_base);
	unsigned long pfn;
	unsigned long kernel_cr3 = this_cpu_read(cpu_tss_rw.tss_ex.smod_cr3);
	unsigned long *entry_ptr;
	int actual_level;
	int ret;

	ret = runpv_mmu_write_trylock();
	if (ret < 0) {
		return ret;
	}

	ret = pw_walk_address(kva, kernel_cr3, PW_LEVEL_PTE, &entry_ptr, &actual_level);
	if (ret < 0 || actual_level != PW_LEVEL_PTE) {
		pv_info(RUNPV_HC_DEBUG, "%s: pw_walk_address fail %d kva %#lx\n", __func__, ret, kva);
		goto out_unlock;
	}
	if (check_shadow_present(*(pte_t *)entry_ptr)) {
		pv_info(RUNPV_HC_DEBUG, "%s: pte %#lx is already shadowed kva %#lx\n", __func__, entry_ptr, kva);
		ret = -EINVAL;
		goto out_unlock;
	}

	runpv_get_host_page(page_array, gfn, &pfn);

	ret = free_one_page(page_buffer, pfn);
	if (ret < 0) {
		pv_info(RUNPV_HC_DEBUG, "%s: free_one_page fail %d\n", __func__, ret);
		goto out_unlock;
	}

	runpv_unbind_host_page(page_array, gfn);

	BUG_ON(check_shadow_present(*(pte_t *)entry_ptr));
	pv_info(RUNPV_HC_DEBUG, "PVM: set pte %#lx from %#lx to 0 kva %#lx\n", entry_ptr, *(pte_t *)entry_ptr, kva);
	WRITE_ONCE(*(pte_t *)entry_ptr, __pte(0));
	flush_tlb_one_kernel(kva);

out_unlock:
	runpv_mmu_write_unlock();
	return ret;
}

long runpv_hc_handle_mark_page_pt(unsigned long gfn, unsigned long kva, bool mark)
{
	struct runpv_page_array *page_array = (struct runpv_page_array *)this_cpu_read(cpu_tss_rw.tss_ex.page_array_base);
	int ret = 0;

	ret = runpv_mmu_write_trylock();
	if (ret < 0) {
		return ret;
	}

	if (mark) {
		unsigned long kernel_cr3 = this_cpu_read(cpu_tss_rw.tss_ex.smod_cr3);
		unsigned long kva_pa;
		ret = va_to_pa(kernel_cr3, kva, &kva_pa);
		BUG_ON(ret < 0);
		runpv_mark_page_pt(page_array, gfn, kva_pa >> PAGE_SHIFT);
	} else {
		runpv_unmark_page_pt(page_array, gfn);
	}

	runpv_mmu_write_unlock();
	return ret;
}

// RUNPV-TODO: pcid tlb flush

long runpv_hc_handle_tlb_invlpg(unsigned long addr)
{
	pv_info(RUNPV_HC_DEBUG, "PVM: tlb invlpg %#lx\n", addr);
	
	return 0;
}

long runpv_hc_handle_tlb_flush(void)
{
	pv_info(RUNPV_HC_DEBUG, "PVM: tlb flush\n");
	
	__native_tlb_flush_global(native_read_cr4());
	
	return 0;
}

long runpv_hc_handle_tlb_flush_current(void)
{
	pv_info(RUNPV_HC_DEBUG, "PVM: tlb flush current\n");
	
	return 0;
}

__visible noinstr long do_fast_hypercall(long nr, long a0, long a1, long a2) {
	switch (nr) {
	case RUNPV_HC_SET_PTE:
		return runpv_hc_handle_set_pte(a0, a1);
	case RUNPV_HC_BIND_HOST_PAGE:
		return runpv_hc_handle_bind_host_page(a0, a1, a2);
	case RUNPV_HC_UNBIND_HOST_PAGE:
		return runpv_hc_handle_unbind_host_page(a0, a1);
	case RUNPV_HC_MARK_PAGE_PT:
		return runpv_hc_handle_mark_page_pt(a0, a1, a2);
	case RUNPV_HC_TLB_FLUSH:
		return runpv_hc_handle_tlb_flush();
	case RUNPV_HC_TLB_FLUSH_CURRENT:
		return runpv_hc_handle_tlb_flush_current();
	case RUNPV_HC_TLB_INVLPG:
		return runpv_hc_handle_tlb_invlpg(a0);
	default:
		pr_err("PVM: unknown fast hypercall: %#lx\n", nr);
		return -ENOSYS;
	}

	return 0;
}
