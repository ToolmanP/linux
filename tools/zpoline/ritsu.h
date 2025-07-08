#ifndef __RITSU_H
#define __RITSU_H
#include <asm/processor-flags.h>
#include <asm-generic/unistd.h>
#include <asm/unistd.h>

#define __SYS_PVCS_SET_TLS 457
#define __SYS_YUI_DIRECT 458

#define PVCS_event_flags 0
#define PVCS_event_errcode 8
#define PVCS_event_vector 12
#define PVCS_cr2 16
#define PVCS_reserved0 24
#define PVCS_user_cs 64
#define PVCS_user_ss 66
#define PVCS_reserved1 68
#define PVCS_reserved2 72
#define PVCS_user_gsbase 80
#define PVCS_eflags 88
#define PVCS_pkru 92
#define PVCS_rip 96
#define PVCS_rsp 104
#define PVCS_rcx 112
#define PVCS_r11 120
#define PVCS_kernel_gsbase 128
#define PVCS_switch_flags 136
#define PVCS_kernel_rflags 144
#define PVCS_kernel_rsp 152
#define PVCS_user_gsbase_direct 160
#define PVCS_user_rsp_direct 168
#define PVCS_dstack 176
#define PVCS_yui_addr 184

#define GDT_ENTRY_DEFAULT_USER_DS 5
#define GDT_ENTRY_DEFAULT_USER_CS 6
#define __USER_DS (GDT_ENTRY_DEFAULT_USER_DS * 8 + 3)
#define __USER_CS (GDT_ENTRY_DEFAULT_USER_CS * 8 + 3)

#define SWITCH_ENTER_EFLAGS_ALLOWED                                         \
	(X86_EFLAGS_FIXED | X86_EFLAGS_IF | X86_EFLAGS_TF | X86_EFLAGS_RF | \
	 X86_EFLAGS_AC | X86_EFLAGS_OF | X86_EFLAGS_DF | X86_EFLAGS_SF |    \
	 X86_EFLAGS_ZF | X86_EFLAGS_AF | X86_EFLAGS_PF | X86_EFLAGS_CF |    \
	 X86_EFLAGS_ID | X86_EFLAGS_NT)

#define DIRECTCALL_ENTER_MASK                                             \
	(X86_EFLAGS_CF | X86_EFLAGS_PF | X86_EFLAGS_AF | X86_EFLAGS_ZF |   \
	 X86_EFLAGS_SF | X86_EFLAGS_TF | X86_EFLAGS_IF | X86_EFLAGS_DF |   \
	 X86_EFLAGS_OF | X86_EFLAGS_IOPL | X86_EFLAGS_NT | X86_EFLAGS_RF | \
	 X86_EFLAGS_AC | X86_EFLAGS_ID)

#define DIRECT_CALL_LEAVE_MASK (X86_EFLAGS_FIXED | X86_EFLAGS_IF)
#define SWITCH_ENTER_EFLAGS_FIXED (X86_EFLAGS_FIXED | X86_EFLAGS_IF)

#ifndef __ASSEMBLY__
#include <stdbool.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;

struct pvm_vcpu_struct {
	/*
   * This flag is only used in supervisor mode, with only bit 8 and
   * bit 9 being valid. The other bits are reserved.
   */
	u64 event_flags;
	u32 event_errcode;
	u32 event_vector;
	u64 cr2;
	u64 reserved0[5];

	/*
   * For the event from supervisor mode with vector >=32, only eflags,
   * rip, rsp, rcx and r11 are saved, and others keep untouched.
   */
	u16 user_cs, user_ss;
	u32 reserved1;
	u64 reserved2;
	u64 user_gsbase;
	u32 eflags;
	u32 pkru;
	u64 rip;
	u64 rsp;
	u64 rcx;
	u64 r11;
	u64 kernel_gsbase;
	u64 switch_flags;
	u64 kernel_rflags;
	u64 kernel_rsp;
	u64 user_gsbase_direct;
	u64 user_rsp_direct;
	u64 dstack;
	u64 yui_addr;
} __attribute__((aligned(4096)));

#define PVM_EVENT_FLAGS_EF_BIT 0
#define PVM_EVENT_FLAGS_IF_BIT 9
#define PVM_EVENT_FLAGS_EF _BITUL(PVM_EVENT_FLAGS_EF_BIT)
#define PVM_EVENT_FLAGS_IF _BITUL(PVM_EVENT_FLAGS_IF_BIT)
#define __hidden __attribute__((visibility("hidden")))
extern long call_azunya(long a1, long a2, long a3, long a4, long a5, long a6,
			long a7);
#else

#endif
#endif
