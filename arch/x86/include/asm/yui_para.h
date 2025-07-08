/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_X86_YUI_PARA_H
#define __ASM_X86_YUI_PARA_H

#include <linux/sched.h>
#include <asm/pvm_para.h>

#ifndef __ASSEMBLY__

#ifdef CONFIG_YUI_GUEST
DECLARE_PER_CPU_PAGE_ALIGNED(struct pvm_vcpu_struct, pvm_vcpu_struct);

static inline void yui_switch_dstack(struct task_struct *tsk){
	raw_cpu_write(pvm_vcpu_struct.dstack, (u64)tsk->dstack + THREAD_SIZE - 16);
}

void yui_setup_pvcs(int cpu);
int yui_remap_pvcs_tls(struct task_struct *p, int dest_cpu);
void entry_DIRECTCALL_64_yui(void);

#else
static inline void yui_setup_pvcs(int cpu) { }
static inline void yui_switch_dstack(struct task_struct *tsk){
}
#endif

#endif
#endif
