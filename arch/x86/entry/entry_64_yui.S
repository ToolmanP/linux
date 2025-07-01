/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/linkage.h>
#include <asm/segment.h>
#include <asm/asm-offsets.h>
#include <asm/percpu.h>
#include <asm/pvm_para.h>
#include <asm/errno.h>
#include <asm/switcher.h>

#include "calling.h"

.extern do_syscall_64_yui
.extern do_syscall_64

.macro pvm_enable_events
	orq	$PVM_EVENT_FLAGS_EF, PER_CPU_VAR(pvm_vcpu_struct + PVCS_event_flags)
	btq	$PVM_EVENT_FLAGS_EP_BIT, PER_CPU_VAR(pvm_vcpu_struct + PVCS_event_flags)
	jnc	.L_no_event_pending_\@
	pushq	%rax
	movq	$PVM_HC_EVENT_WIN, %rax
	call	pvm_hypercall
	popq	%rax
.L_no_event_pending_\@:
.endm

.macro pvm_disable_events
	btrq	$PVM_EVENT_FLAGS_EF_BIT, PER_CPU_VAR(pvm_vcpu_struct + PVCS_event_flags)
.endm

SYM_CODE_START(entry_DIRECTCALL_64_yui)
	UNWIND_HINT_ENTRY
	ENDBR

	pushq %r13
	pushq %r12
	pvm_enable_events
	xorq $SWITCH_FLAGS_MOD_TOGGLE, PER_CPU_VAR(pvm_vcpu_struct + PVCS_switch_flags)
	orq  $SWITCH_ENTER_EFLAGS_FIXED, PER_CPU_VAR(pvm_vcpu_struct + PVCS_kernel_rflags)
	call do_syscall_64_yui

	andq $(~(X86_EFLAGS_IF)), PER_CPU_VAR(pvm_vcpu_struct + PVCS_kernel_rflags)
	pvm_disable_events
	xorq $SWITCH_FLAGS_MOD_TOGGLE, PER_CPU_VAR(pvm_vcpu_struct + PVCS_switch_flags)
	movq PER_CPU_VAR(pvm_vcpu_struct + PVCS_user_gsbase), %r13
	wrgsbase %r13
	popq %r12
	popq %r13
	RET
SYM_FUNC_END(entry_DIRECTCALL_64_yui)
