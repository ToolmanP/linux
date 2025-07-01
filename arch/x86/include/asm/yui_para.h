/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_X86_YUI_PARA_H
#define __ASM_X86_YUI_PARA_H

#include <asm/pvm_para.h>

#ifndef __ASSEMBLY__

#ifdef CONFIG_YUI_GUEST
void yui_setup_pvcs(int cpu);
int yui_remap_pvcs_tls(struct task_struct *p, int dest_cpu);
void entry_DIRECTCALL_64_yui(void);

#else
static inline void yui_setup_pvcs(int cpu) { }
#endif

#endif
#endif
