/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_ASM_X86_PVM_PARA_H
#define _UAPI_ASM_X86_PVM_PARA_H
#include <linux/const.h>
#include <asm/page_types.h>

/*
 * The CPUID instruction in PVM guest can't be trapped and emulated,
 * so PVM guest should use the following two instructions instead:
 * "invlpg 0xffffffffff4d5650; cpuid;"
 *
 * PVM_SYNTHETIC_CPUID is supposed to not trigger any trap in the real or
 * virtual x86 kernel mode and is also guaranteed to trigger a trap in the
 * underlying hardware user mode for the hypervisor emulating it. The
 * hypervisor emulates both of the basic instructions, while the INVLPG is
 * often emulated as an NOP since 0xffffffffff4d5650 is normally out of the
 * allowed linear address ranges.
 */
#define PVM_SYNTHETIC_CPUID		0x0f,0x01,0x3c,0x25,0x50, \
					0x56,0x4d,0xff,0x0f,0xa2
#define PVM_SYNTHETIC_CPUID_ADDRESS	0xffffffffff4d5650

/*
 * The vendor signature 'PVM' is returned in ebx. It should be used to
 * determine that a VM is running under PVM.
 */
#define PVM_CPUID_SIGNATURE		0x4d5650

/*
 * PVM virtual MSRS falls in the range 0x4b564df0-0x4b564dff, and it should not
 * conflict with KVM, see arch/x86/include/uapi/asm/kvm_para.h
 */
#define PVM_VIRTUAL_MSR_MAX_NR		15
#define PVM_VIRTUAL_MSR_BASE		0x4b564df0
#define PVM_VIRTUAL_MSR_MAX		(PVM_VIRTUAL_MSR_BASE+PVM_VIRTUAL_MSR_MAX_NR)

#define MSR_PVM_LINEAR_ADDRESS_RANGE	0x4b564df0
#define MSR_PVM_VCPU_STRUCT		0x4b564df1
#define MSR_PVM_SWITCH_CR3		0x4b564df2
#define MSR_PVM_SUPERVISOR_RSP		0x4b564df3
#define MSR_PVM_EVENT_ENTRY		0x4b564df4
#define MSR_PVM_RETU_RIP		0x4b564df5
#define MSR_PVM_RETS_RIP		0x4b564df6

#define PVM_HC_SPECIAL_MAX_NR		(256)
#define PVM_HC_SPECIAL_BASE		(0x17088200)
#define PVM_HC_SPECIAL_MAX		(PVM_HC_SPECIAL_BASE+PVM_HC_SPECIAL_MAX_NR)

#define PVM_HC_LOAD_PGTBL		(PVM_HC_SPECIAL_BASE+0)
#define PVM_HC_EVENT_WIN		(PVM_HC_SPECIAL_BASE+1)
#define PVM_HC_IRQ_HALT			(PVM_HC_SPECIAL_BASE+2)
#define PVM_HC_TLB_FLUSH		(PVM_HC_SPECIAL_BASE+3)
#define PVM_HC_TLB_FLUSH_CURRENT	(PVM_HC_SPECIAL_BASE+4)
#define PVM_HC_TLB_INVLPG		(PVM_HC_SPECIAL_BASE+5)
#define PVM_HC_LOAD_GS			(PVM_HC_SPECIAL_BASE+6)
#define PVM_HC_RDMSR			(PVM_HC_SPECIAL_BASE+7)
#define PVM_HC_WRMSR			(PVM_HC_SPECIAL_BASE+8)
#define PVM_HC_LOAD_TLS			(PVM_HC_SPECIAL_BASE+9)
#define PVM_HC_SYNC_PAGES		(PVM_HC_SPECIAL_BASE+10)
#define PVM_HC_SYNC_FREE_PAGES	(PVM_HC_SPECIAL_BASE+11)
#define PVM_HC_SET_PAGE_OFFSET_BASE	(PVM_HC_SPECIAL_BASE+12)
#define PVM_HC_VIRTIO_NOTIFY (PVM_HC_SPECIAL_BASE+13)
#define PVM_HC_MARK_KPFN (PVM_HC_SPECIAL_BASE+14)
#define PVM_HC_FREE_KPFN (PVM_HC_SPECIAL_BASE+15)
#define PVM_HC_MMU_SET_PTE (PVM_HC_SPECIAL_BASE+16)

#define RUNPV_HC_SPECIAL_MAX_NR		(256)
#define RUNPV_HC_SPECIAL_BASE		PVM_HC_SPECIAL_MAX
#define RUNPV_HC_SPECIAL_MAX		(RUNPV_HC_SPECIAL_BASE+RUNPV_HC_SPECIAL_MAX_NR)

#define RUNPV_HC_SET_PTE			(RUNPV_HC_SPECIAL_BASE+0)
#define RUNPV_HC_BIND_HOST_PAGE		(RUNPV_HC_SPECIAL_BASE+1)
#define RUNPV_HC_UNBIND_HOST_PAGE	(RUNPV_HC_SPECIAL_BASE+2)
#define RUNPV_HC_MARK_PAGE_PT		(RUNPV_HC_SPECIAL_BASE+3)
#define RUNPV_HC_TLB_FLUSH			(RUNPV_HC_SPECIAL_BASE+4)
#define RUNPV_HC_TLB_FLUSH_CURRENT	(RUNPV_HC_SPECIAL_BASE+5)
#define RUNPV_HC_TLB_INVLPG		(RUNPV_HC_SPECIAL_BASE+6)

#define RUNPV_USER_GSBASE_MAGIC (0xfff)

/*
 * PVM_EVENT_FLAGS_EF
 *	- Event enable flag. The flag is set to respond to events;
 *	  and cleared to inhibit events. When the hypervisor try to inject
 *	  an event except for NMI with PVM_EVENT_FLAGS_EF cleared, it will
 *	  morph it to triple-fault.
 *
 * PVM_EVENT_FLAGS_EP
 *	- Event pending flag. The hypervisor sets it if it fails to inject
 *	  an event (NMI) to the VCPU due to the event-enable flag being
 *	  cleared in supervisor mode.
 *
 * PVM_EVENT_FLAGS_IF
 *	- Interrupt enable flag. The flag is set to respond to maskable
 *	  external interrupts; and cleared to inhibit maskable external
 *	  interrupts.
 *
 * PVM_EVENT_FLAGS_IP
 *	- interrupt pending flag. The hypervisor sets it if it fails to inject
 *	  a maskable event to the VCPU due to the interrupt-enable flag being
 *	  cleared in supervisor mode.
 */
#define PVM_EVENT_FLAGS_EF_BIT		0
#define PVM_EVENT_FLAGS_EF		_BITUL(PVM_EVENT_FLAGS_EF_BIT)
#define PVM_EVENT_FLAGS_EP_BIT		1
#define PVM_EVENT_FLAGS_EP		_BITUL(PVM_EVENT_FLAGS_EP_BIT)
#define PVM_EVENT_FLAGS_IP_BIT		8
#define PVM_EVENT_FLAGS_IP		_BITUL(PVM_EVENT_FLAGS_IP_BIT)
#define PVM_EVENT_FLAGS_IF_BIT		9
#define PVM_EVENT_FLAGS_IF		_BITUL(PVM_EVENT_FLAGS_IF_BIT)

#define RUNPV_INTR_MASK_IF_BIT 0
#define RUNPV_INTR_MASK_IF _BITUL(RUNPV_INTR_MASK_IF_BIT)
#define RUNPV_INTR_MASK_NMI_BIT 1
#define RUNPV_INTR_MASK_NMI _BITUL(RUNPV_INTR_MASK_NMI_BIT)

#define PVM_LOAD_PGTBL_FLAGS_TLB	_BITUL(0)
#define PVM_LOAD_PGTBL_FLAGS_LA57	_BITUL(1)

#ifndef __ASSEMBLY__
#include <linux/spinlock.h>
/*
 * PVM event delivery saves the information about the event and the old context
 * into the PVCS structure if the event is from user mode or from supervisor
 * mode with vector >=32. And ERETU synthetic instruction reads the return
 * state from the PVCS structure to restore the old context.
 */
struct pvm_vcpu_struct {
	/*
	 * This flag is only used in supervisor mode, with only bit 8 and
	 * bit 9 being valid. The other bits are reserved.
	 */
	u64 event_flags; /* The stuff that only works in kernel mode. When doing VM-exit in VM-kernel mode, the hypervisor updates vm_rflags based on it. */
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
	u64 switch_flags; // for switcher to identify whether to be back to host?
	u64 vm_rflags; // This is the one that really decides the event handling.
	u64 kernel_rsp;
	u64 user_rsp;
	u64 intr_mask;
	u64 yui_entry;
  u64 pid;
} __aligned(PAGE_SIZE);

#define RUNPV_PAGE_ARRAY_SHADOW_COUNT	(16)
#define RUNPV_PAGE_ARRAY_SLOT_NR (64)

typedef struct {
	unsigned long val;
} runpv_pae_t;

#define RUNPV_PAE_PST			(_BITUL(0))
#define RUNPV_PAE_PT			(_BITUL(11))

#define RUNPV_PAE_PFN_OFFSET	(12)

#define RUNPV_PAE_PHYSICAL_MASK	(~((_BITUL(RUNPV_PAE_PFN_OFFSET) - 1)))

#define runpv_make_pae(pfn, flags) (runpv_pae_t){ .val = (pfn << RUNPV_PAE_PFN_OFFSET) | (flags) }
#define runpv_pae_to_pfn(pae) (((pae).val & RUNPV_PAE_PHYSICAL_MASK) >> RUNPV_PAE_PFN_OFFSET)

#define runpv_check_pae_pst(pae) ((pae).val & RUNPV_PAE_PST)
#define runpv_check_pae_pt(pae) ((pae).val & RUNPV_PAE_PT)

struct runpv_page_array_entry {
	runpv_pae_t pfn;
	unsigned long shadow_pfn[RUNPV_PAGE_ARRAY_SHADOW_COUNT];
};

struct runpv_page_array_slot {
	unsigned long gfn_start;
	unsigned long gfn_size;
	struct runpv_page_array_entry entries[0];
};

#define size_of_runpv_page_array_slot(gfn_count) \
	(sizeof(struct runpv_page_array_slot) + \
	 sizeof(struct runpv_page_array_entry) * (gfn_count))

struct runpv_page_array {
	struct runpv_page_array_slot *slots[RUNPV_PAGE_ARRAY_SLOT_NR];
};


#define RUNPV_PAGE_BUFFER_PAGE_NR 1024

struct runpv_page_buffer {
	spinlock_t lock;
	unsigned long magic;
	unsigned long page_head, page_rear;
	unsigned long free_head, free_rear;
	unsigned long alloc_pfns[RUNPV_PAGE_BUFFER_PAGE_NR];
	unsigned long free_pfns[RUNPV_PAGE_BUFFER_PAGE_NR];
};

#define pv_print(debug_switch, print_func, fmt, ...) \
	do { \
		if (debug_switch) \
			print_func("%s:%d: " fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define pv_debug(debug_switch, ...) pv_print(debug_switch, pr_debug, __VA_ARGS__)
#define pv_info(debug_switch, ...) pv_print(debug_switch, pr_info, __VA_ARGS__)
#define pv_warn(debug_switch, ...) pv_print(debug_switch, pr_warn, __VA_ARGS__)
#define pv_err(debug_switch, ...) pv_print(debug_switch, pr_err, __VA_ARGS__)

#endif /* __ASSEMBLY__ */

#endif /* _UAPI_ASM_X86_PVM_PARA_H */
