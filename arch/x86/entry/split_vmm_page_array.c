// SPDX-License-Identifier: GPL-2.0-only
/*
 * split_vmm_page_array.c - Page array management for split VMM
 */

#include <linux/errno.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <uapi/asm/pvm_para.h>

#include "split_vmm.h"

#define PAGE_ARRAY_DEBUG 0

static inline struct runpv_page_array_slot *get_slot(struct runpv_page_array *page_array, unsigned long gfn)
{
	for (int i = 0; i < RUNPV_PAGE_ARRAY_SLOT_NR; i++) {
		if (page_array->slots[i] != NULL) {
			if (gfn >= page_array->slots[i]->gfn_start && gfn < page_array->slots[i]->gfn_start + page_array->slots[i]->gfn_size) {
				return page_array->slots[i];
			}
		}
	}
	pr_err("gfn %#lx is not in any slot\n", gfn);
	BUG();
	return NULL;
}

static inline void bind_host_page(struct runpv_page_array_slot *slot, unsigned long gfn, unsigned long pfn)
{
	runpv_pae_t *pae = &slot->entries[gfn - slot->gfn_start].pfn;
	BUG_ON(runpv_check_pae_pst(*pae));
	*pae = runpv_make_pae(pfn, RUNPV_PAE_PST);
	pv_info(PAGE_ARRAY_DEBUG, "bind gfn %#lx to %#lx\n", gfn, pfn);
}

static inline void get_host_page(struct runpv_page_array_slot *slot, unsigned long gfn, unsigned long *pfn)
{
	runpv_pae_t *pae = &slot->entries[gfn - slot->gfn_start].pfn;
	BUG_ON(!runpv_check_pae_pst(*pae));
	*pfn = runpv_pae_to_pfn(*pae);
	pv_info(PAGE_ARRAY_DEBUG, "get gfn %#lx to pfn %#lx\n", gfn, *pfn);
}

static inline void unbind_host_page(struct runpv_page_array_slot *slot, unsigned long gfn)
{
	runpv_pae_t *pae = &slot->entries[gfn - slot->gfn_start].pfn;
	BUG_ON(!runpv_check_pae_pst(*pae));
	*pae = runpv_make_pae(0, 0);
	pv_info(PAGE_ARRAY_DEBUG, "unbind gfn %#lx\n", gfn);
}

void runpv_register_page_array_slot(struct runpv_page_array *page_array, unsigned long gfn_start, unsigned long gfn_size)
{
	struct runpv_page_array_slot *slot;
	size_t slot_size;
	int i;

	/* Find an empty slot */
	for (i = 0; i < RUNPV_PAGE_ARRAY_SLOT_NR; i++) {
		if (page_array->slots[i] == NULL)
			break;
	}

	if (i >= RUNPV_PAGE_ARRAY_SLOT_NR) {
		pr_err("no free slot available for gfn %#lx\n", gfn_start);
		BUG();
		return;
	}

	/* Allocate the slot with entries array */
	slot_size = size_of_runpv_page_array_slot(gfn_size);
	slot = kvzalloc(slot_size, GFP_KERNEL);
	if (!slot) {
		pr_err("failed to allocate page array slot for gfn %#lx\n", gfn_start);
		BUG();
		return;
	}

	slot->gfn_start = gfn_start;
	slot->gfn_size = gfn_size;
	page_array->slots[i] = slot;

	pv_info(PAGE_ARRAY_DEBUG, "registered slot %d for gfn %#lx size %lu\n", i, gfn_start, gfn_size);
}
EXPORT_SYMBOL_GPL(runpv_register_page_array_slot);

void runpv_unregister_page_array_slot(struct runpv_page_array *page_array, unsigned long gfn_start)
{
	struct runpv_page_array_slot *slot;
	int i;

	/* Find the slot */
	for (i = 0; i < RUNPV_PAGE_ARRAY_SLOT_NR; i++) {
		slot = page_array->slots[i];
		if (slot != NULL && slot->gfn_start == gfn_start) {
			page_array->slots[i] = NULL;
			kvfree(slot);
			pv_info(PAGE_ARRAY_DEBUG, "unregistered slot %d for gfn %#lx\n", i, gfn_start);
			return;
		}
	}

	pr_err("slot not found for gfn %#lx\n", gfn_start);
	BUG();
}
EXPORT_SYMBOL_GPL(runpv_unregister_page_array_slot);

void runpv_bind_host_page(struct runpv_page_array *page_array, unsigned long gfn, unsigned long pfn)
{
	struct runpv_page_array_slot *slot = get_slot(page_array, gfn);
	bind_host_page(slot, gfn, pfn);
}
EXPORT_SYMBOL_GPL(runpv_bind_host_page);

void runpv_get_host_page(struct runpv_page_array *page_array, unsigned long gfn, unsigned long *pfn)
{
	struct runpv_page_array_slot *slot = get_slot(page_array, gfn);
	get_host_page(slot, gfn, pfn);
}
EXPORT_SYMBOL_GPL(runpv_get_host_page);

void runpv_unbind_host_page(struct runpv_page_array *page_array, unsigned long gfn)
{
	struct runpv_page_array_slot *slot = get_slot(page_array, gfn);
	unbind_host_page(slot, gfn);
}
EXPORT_SYMBOL_GPL(runpv_unbind_host_page);
