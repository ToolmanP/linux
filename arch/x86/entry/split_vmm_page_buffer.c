// SPDX-License-Identifier: GPL-2.0-only
/*
 * split_vmm_page_buffer.c - Page buffer management for split VMM
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <uapi/asm/pvm_para.h>

#include "split_vmm.h"

#define PAGE_BUFFER_DEBUG 0

int get_one_page(struct runpv_page_buffer *page_buffer, unsigned long *out_page)
{
	unsigned long page = 0;
	unsigned long next;
	int ret = -ENODATA;

	if (!page_buffer || !out_page)
		return -EINVAL;

	spin_lock(&page_buffer->lock);

	/* Consume from free ring first */
	if (page_buffer->free_head != page_buffer->free_rear) {
		page = page_buffer->free_pfns[page_buffer->free_head];
		next = (page_buffer->free_head + 1) % RUNPV_PAGE_BUFFER_PAGE_NR;
		page_buffer->free_head = next;
		ret = 0;
		goto out_unlock;
	}

	/* Fallback: consume from pages ring */
	if (page_buffer->page_head != page_buffer->page_rear) {
		page = page_buffer->alloc_pfns[page_buffer->page_head];
		next = (page_buffer->page_head + 1) % RUNPV_PAGE_BUFFER_PAGE_NR;
		page_buffer->page_head = next;
		ret = 0;
		goto out_unlock;
	}

out_unlock:
	spin_unlock(&page_buffer->lock);

	if (ret == 0) {
		*out_page = page;
	}

	return ret;
}

int free_one_page(struct runpv_page_buffer *page_buffer, unsigned long page)
{
	unsigned long next;
	int ret = 0;

	if (!page_buffer)
		return -EINVAL;

	spin_lock(&page_buffer->lock);

	next = (page_buffer->free_rear + 1) % RUNPV_PAGE_BUFFER_PAGE_NR;
	if (next == page_buffer->free_head) {
		ret = -ENODATA;
		goto out_unlock;
	}

	page_buffer->free_pfns[page_buffer->free_rear] = page;
	page_buffer->free_rear = next;

out_unlock:
	spin_unlock(&page_buffer->lock);
	return ret;
}
