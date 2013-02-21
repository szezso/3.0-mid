#ifndef _ASM_X86_MWAIT_H
#define _ASM_X86_MWAIT_H

#define MWAIT_SUBSTATE_MASK		0xf
#define MWAIT_CSTATE_MASK		0xf
#define MWAIT_SUBSTATE_SIZE		4
#define MWAIT_MAX_NUM_CSTATES		10

#define CPUID_MWAIT_LEAF		5
#define CPUID5_ECX_EXTENSIONS_SUPPORTED 0x1
#define CPUID5_ECX_INTERRUPT_BREAK	0x2

#define MWAIT_ECX_INTERRUPT_BREAK	0x1

#endif /* _ASM_X86_MWAIT_H */