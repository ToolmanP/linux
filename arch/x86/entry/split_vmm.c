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
#include <asm/runpv_para.h>
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

// RUNPV-TODO: pcid tlb flush

long runpv_hc_handle_tlb_invlpg(unsigned long addr)
{
	asm volatile("invlpg (%0)" ::"r" (addr) : "memory");
	
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

long runpv_hc_handle_mmu_op(long nr, long a0, long a1, long a2, long a3)
{
	struct kvm_vcpu *vcpu = (struct kvm_vcpu *)this_cpu_read(cpu_tss_rw.tss_ex.vcpu_ptr);
	long (*runpv_mmu_op_func)(struct kvm_vcpu *, long, long, long, long, long)
		= (long (*)(struct kvm_vcpu *, long, long, long, long, long))this_cpu_read(cpu_tss_rw.tss_ex.runpv_mmu_op_func);
	long ret = runpv_mmu_op_func(vcpu, nr, a0, a1, a2, a3);
	return ret;
}

__visible noinstr long do_fast_hypercall(long nr, long a0, long a1, long a2, long a3) {
	long ret = 0;

	switch (nr) {
	case RUNPV_HC_MMU_PTE_SET:
	case RUNPV_HC_MMU_PMD_SET:
	case RUNPV_HC_MMU_PUD_SET:
	case RUNPV_HC_MMU_P4D_SET:
	case RUNPV_HC_MMU_PTEP_GET:
	case RUNPV_HC_MMU_PMDP_GET:
	case RUNPV_HC_MMU_PTEP_GET_AND_CLEAR:
	case RUNPV_HC_MMU_PMDP_GET_AND_CLEAR:
	case RUNPV_HC_MMU_PUDP_GET_AND_CLEAR:
	case RUNPV_HC_MMU_PTEP_GET_AND_CLEAR_FULL:
	case RUNPV_HC_MMU_PTEP_SET_WRPROTECT:
	case RUNPV_HC_MMU_PTEP_SET_ACCESS_FLAGS:
	case RUNPV_HC_MMU_PTEP_TEST_AND_CLEAR_YOUNG:
	case RUNPV_HC_MMU_PMDP_HUGE_GET_AND_CLEAR:
	case RUNPV_HC_MMU_PUDP_HUGE_GET_AND_CLEAR:
	case RUNPV_HC_MMU_PMDP_SET_WRPROTECT:
	case RUNPV_HC_MMU_PMDP_ESTABLISH:
	case RUNPV_HC_MMU_PMDP_SET_ACCESS_FLAGS:
	case RUNPV_HC_MMU_PUDP_SET_ACCESS_FLAGS:
	case RUNPV_HC_MMU_PMDP_TEST_AND_CLEAR_YOUNG:
	case RUNPV_HC_MMU_PUDP_TEST_AND_CLEAR_YOUNG:
	case RUNPV_HC_MMU_PMDP_INVALIDATE_AD:
		return runpv_hc_handle_mmu_op(nr, a0, a1, a2, a3);
	case RUNPV_HC_TLB_FLUSH:
		ret = runpv_hc_handle_tlb_flush();
    break;
	case RUNPV_HC_TLB_FLUSH_CURRENT:
		ret = runpv_hc_handle_tlb_flush_current();
    break;
	case RUNPV_HC_TLB_INVLPG:
		ret = runpv_hc_handle_tlb_invlpg(a0);
    break;
	default:
		pr_err("PVM: unknown fast hypercall: %#lx\n", nr);
		return -ENOSYS;
	}
	return ret;
}
