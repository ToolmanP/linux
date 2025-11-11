/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_X86_RUNPV_PARA_H
#define __ASM_X86_RUNPV_PARA_H

#include <linux/sched.h>
#include <asm/pvm_para.h>

#ifndef __ASSEMBLY__

#ifdef CONFIG_RUNPV_GUEST
DECLARE_PER_CPU_PAGE_ALIGNED(struct pvm_vcpu_struct, pvm_vcpu_struct);

void runpv_setup_pvcs(int cpu);
int runpv_remap_pvcs_tls(struct task_struct *p, int dest_cpu);
void entry_DIRECTCALL_64_runpv(void);

#else
static inline void runpv_setup_pvcs(int cpu) { }
static inline int runpv_remap_pvcs_tls(struct task_struct *p, int dest_cpu) {
  return 0;
}
#endif

#endif
#endif
