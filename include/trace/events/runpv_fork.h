/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM runpv_fork

#if !defined(_TRACE_RUNPV_FORK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_RUNPV_FORK_H

#include <linux/tracepoint.h>

TRACE_EVENT(runpv_fork,

	TP_PROTO(pid_t parent_pid, pid_t child_pid, u64 clone_flags, long ret,
		 u64 copy_process_ns, u64 total_ns),

	TP_ARGS(parent_pid, child_pid, clone_flags, ret, copy_process_ns,
		total_ns),

	TP_STRUCT__entry(
		__field(pid_t, parent_pid)
		__field(pid_t, child_pid)
		__field(u64, clone_flags)
		__field(long, ret)
		__field(u64, copy_process_ns)
		__field(u64, total_ns)
	),

	TP_fast_assign(
		__entry->parent_pid = parent_pid;
		__entry->child_pid = child_pid;
		__entry->clone_flags = clone_flags;
		__entry->ret = ret;
		__entry->copy_process_ns = copy_process_ns;
		__entry->total_ns = total_ns;
	),

	TP_printk("parent=%d child=%d flags=%#llx ret=%ld copy_process_ns=%llu total_ns=%llu",
		  __entry->parent_pid, __entry->child_pid,
		  __entry->clone_flags, __entry->ret,
		  __entry->copy_process_ns, __entry->total_ns)
);

TRACE_EVENT(runpv_dup_mmap,

	TP_PROTO(pid_t pid, unsigned long total_vm, int map_count,
		 unsigned int copied_vmas, u64 copy_page_range_ns,
		 u64 flush_tlb_ns, u64 total_ns, int ret),

	TP_ARGS(pid, total_vm, map_count, copied_vmas, copy_page_range_ns,
		flush_tlb_ns, total_ns, ret),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(unsigned long, total_vm)
		__field(int, map_count)
		__field(unsigned int, copied_vmas)
		__field(u64, copy_page_range_ns)
		__field(u64, flush_tlb_ns)
		__field(u64, total_ns)
		__field(int, ret)
	),

	TP_fast_assign(
		__entry->pid = pid;
		__entry->total_vm = total_vm;
		__entry->map_count = map_count;
		__entry->copied_vmas = copied_vmas;
		__entry->copy_page_range_ns = copy_page_range_ns;
		__entry->flush_tlb_ns = flush_tlb_ns;
		__entry->total_ns = total_ns;
		__entry->ret = ret;
	),

	TP_printk("pid=%d total_vm=%lu map_count=%d copied_vmas=%u copy_page_range_ns=%llu flush_tlb_ns=%llu total_ns=%llu ret=%d",
		  __entry->pid, __entry->total_vm, __entry->map_count,
		  __entry->copied_vmas, __entry->copy_page_range_ns,
		  __entry->flush_tlb_ns, __entry->total_ns, __entry->ret)
);

TRACE_EVENT(runpv_copy_pte_range,

	TP_PROTO(pid_t pid, unsigned long start, unsigned int pages,
		 unsigned int present, unsigned int cow_wrprotect,
		 unsigned int nonpresent, unsigned int none,
		 unsigned int retries, u64 total_ns, int ret),

	TP_ARGS(pid, start, pages, present, cow_wrprotect, nonpresent, none,
		retries, total_ns, ret),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(unsigned long, start)
		__field(unsigned int, pages)
		__field(unsigned int, present)
		__field(unsigned int, cow_wrprotect)
		__field(unsigned int, nonpresent)
		__field(unsigned int, none)
		__field(unsigned int, retries)
		__field(u64, total_ns)
		__field(int, ret)
	),

	TP_fast_assign(
		__entry->pid = pid;
		__entry->start = start;
		__entry->pages = pages;
		__entry->present = present;
		__entry->cow_wrprotect = cow_wrprotect;
		__entry->nonpresent = nonpresent;
		__entry->none = none;
		__entry->retries = retries;
		__entry->total_ns = total_ns;
		__entry->ret = ret;
	),

	TP_printk("pid=%d start=%#lx pages=%u present=%u cow_wrprotect=%u nonpresent=%u none=%u retries=%u total_ns=%llu ret=%d",
		  __entry->pid, __entry->start, __entry->pages,
		  __entry->present, __entry->cow_wrprotect,
		  __entry->nonpresent, __entry->none, __entry->retries,
		  __entry->total_ns, __entry->ret)
);

TRACE_EVENT(runpv_exec,

	TP_PROTO(pid_t pid, int ret, u64 exec_mmap_ns, u64 total_ns),

	TP_ARGS(pid, ret, exec_mmap_ns, total_ns),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(int, ret)
		__field(u64, exec_mmap_ns)
		__field(u64, total_ns)
	),

	TP_fast_assign(
		__entry->pid = pid;
		__entry->ret = ret;
		__entry->exec_mmap_ns = exec_mmap_ns;
		__entry->total_ns = total_ns;
	),

	TP_printk("pid=%d ret=%d exec_mmap_ns=%llu total_ns=%llu",
		  __entry->pid, __entry->ret, __entry->exec_mmap_ns,
		  __entry->total_ns)
);

TRACE_EVENT(runpv_exec_mmap,

	TP_PROTO(pid_t pid, unsigned long old_total_vm, int ret,
		 u64 switch_mm_ns, u64 old_mmput_ns, u64 total_ns),

	TP_ARGS(pid, old_total_vm, ret, switch_mm_ns, old_mmput_ns, total_ns),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(unsigned long, old_total_vm)
		__field(int, ret)
		__field(u64, switch_mm_ns)
		__field(u64, old_mmput_ns)
		__field(u64, total_ns)
	),

	TP_fast_assign(
		__entry->pid = pid;
		__entry->old_total_vm = old_total_vm;
		__entry->ret = ret;
		__entry->switch_mm_ns = switch_mm_ns;
		__entry->old_mmput_ns = old_mmput_ns;
		__entry->total_ns = total_ns;
	),

	TP_printk("pid=%d old_total_vm=%lu ret=%d switch_mm_ns=%llu old_mmput_ns=%llu total_ns=%llu",
		  __entry->pid, __entry->old_total_vm, __entry->ret,
		  __entry->switch_mm_ns, __entry->old_mmput_ns,
		  __entry->total_ns)
);

TRACE_EVENT(runpv_exit_mmap,

	TP_PROTO(pid_t pid, unsigned long total_vm, int map_count,
		 u64 unmap_vmas_ns, u64 free_pgtables_ns,
		 u64 remove_vmas_ns, u64 total_ns),

	TP_ARGS(pid, total_vm, map_count, unmap_vmas_ns, free_pgtables_ns,
		remove_vmas_ns, total_ns),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(unsigned long, total_vm)
		__field(int, map_count)
		__field(u64, unmap_vmas_ns)
		__field(u64, free_pgtables_ns)
		__field(u64, remove_vmas_ns)
		__field(u64, total_ns)
	),

	TP_fast_assign(
		__entry->pid = pid;
		__entry->total_vm = total_vm;
		__entry->map_count = map_count;
		__entry->unmap_vmas_ns = unmap_vmas_ns;
		__entry->free_pgtables_ns = free_pgtables_ns;
		__entry->remove_vmas_ns = remove_vmas_ns;
		__entry->total_ns = total_ns;
	),

	TP_printk("pid=%d total_vm=%lu map_count=%d unmap_vmas_ns=%llu free_pgtables_ns=%llu remove_vmas_ns=%llu total_ns=%llu",
		  __entry->pid, __entry->total_vm, __entry->map_count,
		  __entry->unmap_vmas_ns, __entry->free_pgtables_ns,
		  __entry->remove_vmas_ns, __entry->total_ns)
);

#endif /* _TRACE_RUNPV_FORK_H */

#include <trace/define_trace.h>
