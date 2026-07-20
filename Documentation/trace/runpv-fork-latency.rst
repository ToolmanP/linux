.. SPDX-License-Identifier: GPL-2.0

===========================
RunPV fork latency tracing
===========================

The ``runpv_fork`` tracepoint group separates the cost of constructing a
child address space from the cost of destroying or replacing it.  These
generic events provide a baseline for native Linux, PVM, regular
hardware-virtualized guests, and RunPV.  No timestamps are read until an
event is enabled.

Test matrix
===========

Use equivalent kernel configurations and benchmark settings for every case.
A useful minimum matrix is:

* native ``fork+exit`` and ``fork+execve``;
* a regular hardware-virtualized guest;
* a PVM guest;
* RunPV scalar and batched guests instrumented with the RunPV tracing patch.

Pin the benchmark to one vCPU, warm it up before tracing, and collect multiple
short samples.  Enabling a tracepoint has a measurable cost, so enable the
same guest-side event set in every compared guest or native run.

Guest or native capture
=======================

Mount tracefs if it is not already mounted, clear the ring buffer, and enable
the group::

  mount -t tracefs nodev /sys/kernel/tracing
  cd /sys/kernel/tracing
  echo 0 > tracing_on
  echo nop > current_tracer
  echo mono > trace_clock
  echo 32768 > buffer_size_kb
  echo > trace
  echo 1 > events/runpv_fork/enable
  echo 1 > tracing_on

Run one benchmark sample, then stop and save the trace::

  taskset -c 0 <fork-exit-or-fork-execve-command>
  echo 0 > tracing_on
  cat trace > /tmp/runpv-guest-fork.trace

If tracefs is already mounted, the ``mount`` command is unnecessary.  Avoid
running unrelated fork-heavy workloads during an unfiltered capture.

Event interpretation
====================

``runpv_fork``
  End-to-end ``kernel_clone()`` and its ``copy_process()`` portion.  For
  ``CLONE_VFORK``, total time also contains the parent's wait for the child.

``runpv_dup_mmap``
  Total VMA duplication, accumulated ``copy_page_range()`` time, and the
  final parent TLB flush.  A large ``flush_tlb_ns`` points toward lazy-MMU or
  TLB batching work rather than clone-PTE installation.

``runpv_copy_pte_range``
  One event per PTE page, with present, COW-write-protected, non-present, and
  empty entry counts.  Compare ``total_ns / present`` between configurations
  and sum the event durations per fork.

``runpv_exec`` and ``runpv_exec_mmap``
  The committed exec phase and address-space replacement.  ``old_mmput_ns``
  includes destruction of the forked address space and can be correlated
  with ``runpv_exit_mmap``.

``runpv_exit_mmap``
  Separates leaf unmapping, page-table/TLB teardown, and VMA removal.  If
  ``free_pgtables_ns`` dominates ``fork+exit`` or ``fork+execve``, optimizing
  only the fork-side PTE installs cannot close the performance gap.
