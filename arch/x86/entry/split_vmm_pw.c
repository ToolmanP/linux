// SPDX-License-Identifier: GPL-2.0-only
/*
 * split_vmm_pw.c - Page Walker Implementation for 4-level page tables
 * Supports PGD -> P4D -> PUD -> PMD -> PTE hierarchy
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <asm/pgtable_types.h>
#include <asm/io.h>

#include "split_vmm.h"

#define PW_DEBUG 0

/**
 * pw_get_entry - Get page table entry for given address at specified level
 * @addr: Virtual address to walk
 * @page_table: Physical address of page table page
 * @level: Page table level (4=PGD, 3=P4D, 2=PUD, 1=PMD, 0=PTE)
 * 
 * Returns: Page table entry value, or 0 if invalid parameters
 */
static unsigned long pw_get_entry(unsigned long addr, unsigned long page_table, int level)
{
	unsigned long *table;
	unsigned long index;
	unsigned long shift;
	
	/* Validate level */
	if (level < PW_LEVEL_PTE || level > PW_LEVEL_PGD)
		return 0;
	
	/* Calculate shift amount based on level */
	switch (level) {
	case PW_LEVEL_PGD:
		shift = PW_PGD_SHIFT;
		break;
	case PW_LEVEL_PUD:
		shift = PW_PUD_SHIFT;
		break;
	case PW_LEVEL_PMD:
		shift = PW_PMD_SHIFT;
		break;
	case PW_LEVEL_PTE:
		shift = PW_PTE_SHIFT;
		break;
	default:
		return 0;
	}
	
	/* Extract index from virtual address */
	index = (addr >> shift) & PW_INDEX_MASK;
	
	/* Map page table page to virtual address */
	table = (unsigned long *)__va(page_table & PW_PHYS_MASK);
	
	/* Return the entry */
	return table[index];
}

/**
 * pw_extract_next_level - Extract next level page table address from entry
 * @entry: Page table entry
 * @level: Current level
 * 
 * Returns: Physical address of next level page table, or 0 if not present/invalid
 */
static unsigned long pw_extract_next_level(unsigned long entry, int level)
{
	/* Check if entry is present */
	if (!(entry & _PAGE_PRESENT))
		return 0;
	
	/* For PTE level, there is no next level */
	if (level == PW_LEVEL_PTE)
		return 0;
	
	/* Check for large pages at PMD/PUD levels */
	if ((level == PW_LEVEL_PMD || level == PW_LEVEL_PUD) && (entry & _PAGE_PSE))
		return 0; /* Large page, no next level */
	
	/* Extract physical address of next level page table */
	return entry & PW_PHYS_MASK;
}


/**
 * pw_is_present - Check if page table entry is present
 * @entry: Page table entry
 * 
 * Returns: true if present, false otherwise
 */
static bool pw_is_present(unsigned long entry)
{
	return !!(entry & _PAGE_PRESENT);
}

/**
 * pw_is_large_page - Check if entry represents a large page
 * @entry: Page table entry
 * @level: Page table level
 * 
 * Returns: true if large page, false otherwise
 */
static bool pw_is_large_page(unsigned long entry, int level)
{
	if (!pw_is_present(entry))
		return false;
	
	/* Large pages only possible at PMD (2MB) and PUD (1GB) levels */
	if (level != PW_LEVEL_PMD && level != PW_LEVEL_PUD)
		return false;
	
	return !!(entry & _PAGE_PSE);
}

/**
 * pw_explain_pte - Helper function to explain/decode a page table entry
 * @entry: Page table entry to explain
 * @level: Page table level
 * 
 * Prints detailed information about the page table entry using pr_info
 */
static void pw_explain_pte(unsigned long entry, int level)
{
	const char *level_names[] = {"PTE", "PMD", "PUD", "P4D", "PGD"};
	
	/* Basic entry information */
	pr_info("PW: %s Entry: 0x%016lx", 
		(level >= PW_LEVEL_PTE && level <= PW_LEVEL_PGD) ? level_names[level] : "UNK", entry);
	
	/* Present bit */
	pr_info("PW:   Present: %s", pw_is_present(entry) ? "Yes" : "No");
	
	if (!pw_is_present(entry)) {
		pr_info("PW:   (Non-present entry)");
		return;
	}
	
	/* Physical address */
	pr_info("PW:   Physical Address: 0x%016lx", entry & PW_PHYS_MASK);
	
	/* Large page check */
	if (pw_is_large_page(entry, level)) {
		pr_info("PW:   Large Page: Yes");
	}
	
	/* Permission bits */
	pr_info("PW:   Permissions: %s %s %s",
		(entry & _PAGE_RW) ? "RW" : "RO",
		(entry & _PAGE_USER) ? "USER" : "KERNEL",
		(entry & _PAGE_NX) ? "NX" : "EXEC");
	
	/* Cache attributes */
	pr_info("PW:   Cache: PCD=%s PWT=%s",
		(entry & _PAGE_PCD) ? "1" : "0",
		(entry & _PAGE_PWT) ? "1" : "0");
	
	/* Additional flags */
	pr_info("PW:   Flags:%s%s%s",
		(entry & _PAGE_ACCESSED) ? " ACCESSED" : "",
		(entry & _PAGE_DIRTY) ? " DIRTY" : "",
		(entry & _PAGE_GLOBAL) ? " GLOBAL" : "");
}

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
		    unsigned long **entry_ptr, int *actual_level)
{
	unsigned long current_table = cr3 & PW_PHYS_MASK;
	unsigned long next_table;
	unsigned long *table;
	unsigned long entry;
	unsigned long index;
	unsigned long shift;
	int level;

	pv_info(PW_DEBUG, "PW: walk_address: addr %#lx, cr3 %#lx, target_level %d\n", addr, cr3, target_level);
	
	/* Validate input parameters */
	if (!entry_ptr || !actual_level)
		return -1;
	
	if (target_level < PW_LEVEL_PTE || target_level > PW_LEVEL_PGD)
		return -1;
	
	/* Initialize output parameters */
	*entry_ptr = NULL;
	*actual_level = -1;
	
	/* Walk from PGD down to target level */
	for (level = PW_LEVEL_PGD; level > target_level; level--) {
		entry = pw_get_entry(addr, current_table, level);
		pv_info(PW_DEBUG, "PW: level: %d, entry: %#lx\n", level, entry);

		/* Check if entry is present */
		if (!pw_is_present(entry))
			break;
		
		/* Check for large pages that terminate the walk early */
		if (pw_is_large_page(entry, level)) {
			if (level == target_level + 1) {
				pv_info(PW_DEBUG, "PW: large page at level %d, entry: %#lx\n", level, entry);
				break;
			} else {
				/* Large page above our target level - can't continue */
				return -1;
			}
		}
		
		/* Get next level page table */
		next_table = pw_extract_next_level(entry, level);
		pv_info(PW_DEBUG, "PW: current_table: %#lx va %#lx\n", next_table, __va(next_table));
		if (!next_table)
			return -1;
		current_table = next_table;
	}

	pv_info(PW_DEBUG, "PW: after loop, level: %d entry %#lx\n", level, pw_get_entry(addr, current_table, level));

	if (target_level != level)
		return -1;
	
	/* Get final entry pointer at target level */
	switch (target_level) {
	case PW_LEVEL_PGD:
		shift = PW_PGD_SHIFT;
		break;
	case PW_LEVEL_PUD:
		shift = PW_PUD_SHIFT;
		break;
	case PW_LEVEL_PMD:
		shift = PW_PMD_SHIFT;
		break;
	case PW_LEVEL_PTE:
		shift = PW_PTE_SHIFT;
		break;
	default:
		return -1;
	}
	
	index = (addr >> shift) & PW_INDEX_MASK;
	table = (unsigned long *)__va(current_table);
	
	*entry_ptr = &table[index];
	*actual_level = target_level;
	return 0;
}

/**
 * pw_demo_walk - Demonstration function showing page walker usage
 * @addr: Virtual address to demonstrate with
 * @cr3: CR3 register value
 */
static void pw_demo_walk(unsigned long addr, unsigned long cr3)
{
	unsigned long *entry_ptr;
	unsigned long entry;
	int actual_level;
	int level;
	int ret;
	
	pr_info("PW: Walking address %#lx with CR3 %#lx\n", addr, cr3);
	
	/* Walk each level and show information */
	for (level = PW_LEVEL_PGD; level >= PW_LEVEL_PTE; level--) {
		ret = pw_walk_address(addr, cr3, level, &entry_ptr, &actual_level);
		
		if (ret == 0) {
			entry = *entry_ptr;
			pr_info("PW: Level %d (actual level %d):\n", level, actual_level);
			pw_explain_pte(entry, actual_level);
			
			/* Stop if we hit a large page */
			if (pw_is_large_page(entry, actual_level)) {
				pr_info("PW: Large page found at level %d, stopping walk\n", actual_level);
				break;
			}
		} else {
			pr_info("PW: Level %d: Walk failed\n", level);
			break;
		}
	}
}

/**
 * va_to_pa - Translate virtual address to physical address
 * @cr3: CR3 register value (page table root)
 * @va: Virtual address to translate
 * @pa: Output parameter - physical address
 * 
 * Returns: 0 on success, -1 on failure
 */
int va_to_pa(unsigned long cr3, unsigned long va, unsigned long *pa)
{
	unsigned long *entry_ptr;
	unsigned long entry;
	unsigned long phys_base;
	unsigned long offset_mask;
	int actual_level;
	int target_levels[] = {PW_LEVEL_PTE, PW_LEVEL_PMD, PW_LEVEL_PUD};
	int i;
	
	if (!pa)
		return -1;
	
	*pa = 0;
	
	/* Try walking to different levels, starting with PTE */
	for (i = 0; i < 3; i++) {
		if (pw_walk_address(va, cr3, target_levels[i], &entry_ptr, &actual_level) == 0) {
			entry = *entry_ptr;
			
			/* Check if entry is present */
			if (!pw_is_present(entry))
				continue;
			
			/* Handle different page sizes */
			switch (actual_level) {
			case PW_LEVEL_PTE:
				/* 4KB page */
				phys_base = entry & PW_PHYS_MASK;
				offset_mask = 0xFFF; /* 4KB - 1 */
				break;
				
			case PW_LEVEL_PMD:
				/* 2MB large page */
				if (!pw_is_large_page(entry, actual_level))
					continue;
				phys_base = entry & 0x000FFFFFFFE00000UL; /* 2MB aligned */
				offset_mask = 0x1FFFFF; /* 2MB - 1 */
				break;
				
			case PW_LEVEL_PUD:
				/* 1GB huge page */
				if (!pw_is_large_page(entry, actual_level))
					continue;
				phys_base = entry & 0x000FFFFFC0000000UL; /* 1GB aligned */
				offset_mask = 0x3FFFFFFF; /* 1GB - 1 */
				break;
				
			default:
				continue;
			}
			
			*pa = phys_base + (va & offset_mask);
			return 0;
		}
	}
	
	return -1;
}
