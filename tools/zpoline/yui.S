#define __ASSEMBLY__

#include "ritsu.h"

.extern check_kernel_rsp

.text
.hidden jump_to_yui
.global jump_to_yui
.type jump_to_yui, @function
jump_to_yui:

  pushq %rbp
  movq %rsp, %rbp

  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %rcx
  pushq %r8
  pushq %r9
  pushq 0x10(%rbp) /* arg6 is on the stack.  */
  pushq %r11
  pushq %r12
  pushq %r13

  /* get tls_var pvcs using __tls_get_addr*/
  pushq %r10
  pushq %r11
  leaq	pvcs@TLSGD(%rip), %rdi
  call  __tls_get_addr@PLT
  movq  %rax, %r12
  popq %r11
  popq %r10

  /* mask the eflags altogether. This should disable all interrupts. */
  movq $1, PVCS_user_interrupt_shadow(%r12)

  /* save user_gsbase */
  rdgsbase %r13
  pushq %r13

  pushq PVCS_kernel_rsp(%r12)
  /* save the stack pointer */
  movq %rsp, PVCS_user_rsp_direct(%r12)

  movq PVCS_kernel_rsp(%r12), %rsp
  /* setup the kernel gsbase */
  movq PVCS_kernel_gsbase(%r12), %r13
  wrgsbase %r13

  /* Set up the pt_regs structure on the stack */
  pushq	  $__USER_DS  /* pt_regs->ss */
  pushq	  PVCS_user_rsp_direct(%r12) /* pt_regs->sp */
  pushq	  PVCS_eflags(%r12) /* pt_regs->flags */
  pushq	  $__USER_CS  /* pt_regs->cs */
  pushq	  PVCS_rcx(%r12) /* pt_regs->ip */
  pushq	  -1*8(%rbp)    /* pt_regs->ORIG_RAX */
  pushq   -2*8(%rbp)	/* pt_regs->di */
  pushq   -3*8(%rbp)	/* pt_regs->si */
  pushq	  -4*8(%rbp)	/* pt_regs->dx */
  pushq   -5*8(%rbp)	/* pt_regs->cx */
  pushq   %rax		/* pt_regs->ax */
  pushq   -6*8(%rbp)	/* pt_regs->r8 */
  pushq   -7*8(%rbp)	/* pt_regs->r9 */
  pushq   %r10		/* pt_regs->r10 */
  pushq   %r11		/* pt_regs->r11 */
  pushq	  %rbx		/* pt_regs->rbx */
  pushq	  %rbp		/* pt_regs->rbp */
  pushq	  %r12		/* pt_regs->r12 */
  pushq	  %r13		/* pt_regs->r13 */
  pushq	  %r14		/* pt_regs->r14 */
  pushq	  %r15		/* pt_regs->r15 */

  movq %rsp, %rdi
  movslq -1*8(%rbp), %rsi
  /* let's go! */
  call *PVCS_yui_addr(%r12)

  popq %r15		/* pt_regs->r15 */
  popq %r14		/* pt_regs->r14 */
  popq %r13		/* pt_regs->r13 */
  popq %r12		/* pt_regs->r12 */
  popq %rbp		/* pt_regs->rbp */
  popq %rbx		/* pt_regs->rbx */
  popq %r11		/* pt_regs->r11 */
  popq %r10		/* pt_regs->r10 */
  popq %r9		/* pt_regs->r9 */
  popq %r8		/* pt_regs->r8 */
  popq %rax		/* pt_regs->ax */
  popq %rcx		/* pt_regs->cx */
  popq %rdx		/* pt_regs->dx */
  popq %rsi		/* pt_regs->si */
  popq %rdi		/* pt_regs->di */

  movq 4*8(%rsp), %r13
  movq %r13, %rsp

.Ljump_to_yui_postlogue:

  popq %r13
  movq %r13, PVCS_kernel_rsp(%r12) /* restore the kernel rsp */

  popq %r13
  wrgsbase %r13 /* restore the user gsbase from this stack */

  movq $0, PVCS_user_interrupt_shadow(%r12) /* restore the interrupt state, we are safe now */
  popq %r13
  popq %r12
  popq %r11

  addq $7*8, %rsp /* discard the syscall arguments */

  leave
  ret
