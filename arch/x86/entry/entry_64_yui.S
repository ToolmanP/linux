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

.macro pvm_enable_events
	orq	$PVM_EVENT_FLAGS_EF, PER_CPU_VAR(pvm_vcpu_struct + PVCS_event_flags)
.endm

.macro pvm_disable_events
	btrq	$PVM_EVENT_FLAGS_EF_BIT, PER_CPU_VAR(pvm_vcpu_struct + PVCS_event_flags)
.endm

SYM_CODE_START(entry_DIRECTCALL_64_yui)
	UNWIND_HINT_ENTRY
	ENDBR
	pushq %rbp
	movq %rsp, %rbp
	xorq $SWITCH_FLAGS_MOD_TOGGLE, PER_CPU_VAR(pvm_vcpu_struct + PVCS_switch_flags)
	movq $0, PER_CPU_VAR(pvm_vcpu_struct + PVCS_user_interrupt_shadow)
	pvm_enable_events
	call do_syscall_64_yui
	pvm_disable_events
	movq $1, PER_CPU_VAR(pvm_vcpu_struct + PVCS_user_interrupt_shadow)
	xorq $SWITCH_FLAGS_MOD_TOGGLE, PER_CPU_VAR(pvm_vcpu_struct + PVCS_switch_flags)
	leave
	RET
SYM_FUNC_END(entry_DIRECTCALL_64_yui)
