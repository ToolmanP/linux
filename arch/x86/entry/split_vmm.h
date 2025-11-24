// SPDX-License-Identifier: GPL-2.0-only
/*
 * split_vmm.h - Common header for split VMM page buffer, page array, and page walker
 */

#ifndef _ARCH_X86_ENTRY_SPLIT_VMM_H
#define _ARCH_X86_ENTRY_SPLIT_VMM_H

#include <linux/types.h>
#include <uapi/asm/pvm_para.h>

/* Page walker level definitions */
#define PW_LEVEL_PGD	3
#define PW_LEVEL_PUD	2
#define PW_LEVEL_PMD	1
#define PW_LEVEL_PTE	0

/* Address shift amounts for each level */
#define PW_PGD_SHIFT	39
#define PW_PUD_SHIFT	30
#define PW_PMD_SHIFT	21
#define PW_PTE_SHIFT	12

/* Index mask for each level (9 bits = 512 entries) */
#define PW_INDEX_MASK	0x1FF

/* Physical address mask (bits 12-51) */
#define PW_PHYS_MASK	0x000FFFFFFFFFF000UL

/* Page size constants */
#define PW_PAGE_SIZE	4096
#define PW_PAGE_SHIFT	12

/**
 * pw_walk_address - Complete page table walk for a virtual address
 * @addr: Virtual address to walk
 * @cr3: CR3 register value (PGD physical address)
 * @target_level: Target level to walk to (0=PTE, 1=PMD, etc.)
 * @entry_ptr: Output parameter - pointer to the page table entry
 * @actual_level: Output parameter - actual level of the returned entry
 * 
 * Returns: 0 on success, -1 on failure
 */
int pw_walk_address(unsigned long addr, unsigned long cr3, int target_level, 
		    unsigned long **entry_ptr, int *actual_level);

/**
 * va_to_pa - Translate virtual address to physical address
 * @cr3: CR3 register value (page table root)
 * @va: Virtual address to translate
 * @pa: Output parameter - physical address
 * 
 * Returns: 0 on success, -1 on failure
 */
int va_to_pa(unsigned long cr3, unsigned long va, unsigned long *pa);

/* Page buffer functions */
int get_one_page(struct runpv_page_buffer *page_buffer, unsigned long *out_page);
int free_one_page(struct runpv_page_buffer *page_buffer, unsigned long page);

/* Page array functions */
void runpv_bind_host_page(struct runpv_page_array *page_array, unsigned long gfn, unsigned long pfn);
void runpv_get_host_page(struct runpv_page_array *page_array, unsigned long gfn, unsigned long *pfn);
void runpv_unbind_host_page(struct runpv_page_array *page_array, unsigned long gfn);

#endif /* _ARCH_X86_ENTRY_SPLIT_VMM_H */

