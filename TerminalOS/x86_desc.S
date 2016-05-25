# x86_desc.S - Set up x86 segment descriptors, descriptor tables, and interrupt handlers
# vim:ts=4 noexpandtab

#define ASM     1
#include "x86_desc.h"

.text

.globl  ldt_size, tss_size
.globl  gdt_desc, ldt_desc, tss_desc
.globl  tss, tss_desc_ptr, ldt, ldt_desc_ptr
.globl  gdt_ptr
.globl  idt_desc_ptr, idt


#Assembly Exception Handlers:
.globl  divide_error, debug, nmi, int3, overflow, bounds, invalid_op, device_not_available
.globl  doublefault_fn, coprocessor_segment_overrun, invalid_TSS, segment_not_present, stack_segment
.globl  general_protection, page_fault, coprocessor_error, alignment_check, machine_check, simd_coprocessor_error
.globl  pit_handler, asm_test_interrupts, keyboard, rtc_handler

.align 4


tss_size:
	.long tss_bottom - tss - 1

ldt_size:
	.long ldt_bottom - ldt - 1

	.word 0 # Padding
ldt_desc:
	.word KERNEL_LDT
	.long ldt

	.align 4
tss:
_tss:
	.rept 104
	.byte 0
	.endr
tss_bottom:

	.align  16

	.word 0 # Padding
gdt_desc:
	.word	gdt_bottom - gdt -1
	.long	gdt

gdt:
_gdt:

	# First GDT entry cannot be used
	.quad 0

	# NULL entry
	.quad 0

	# Segmentation will not be used
	# CS and DS both are 0-4GB r/w segments
	#
	# The layout is (from Intel IA-32 reference manual):
	#  31        24 23  22  21  20  19   16 15  14 13 12  11   8 7          0
	# |----------------------------------------------------------------------|
	# |            |   | D |   | A |  Seg  |   |  D  |   |      |            |
	# | Base 31:24 | G | / | 0 | V | Limit | P |  P  | S | Type | Base 23:16 |
	# |            |   | B |   | L | 19:16 |   |  L  |   |      |            |
	# |----------------------------------------------------------------------|
	#
	# |----------------------------------------------------------------------|
	# |                                    |                                 |
	# | Base 15:0                          | Segment Limit 15:0              |
	# |                                    |                                 |
	# |----------------------------------------------------------------------|

gdt_ptr:
	# Set up an entry for kernel CS
	.quad 0x00CF9A000000FFFF

	# Set up an entry for kernel DS
	.quad 0x00CF92000000FFFF

	# Set up an entry for user CS
	.quad 0x00CFFA000000FFFF

	# Set up an entry for user DS
	.quad 0x00CFF2000000FFFF

	# Set up an entry for TSS
tss_desc_ptr:
	.quad 0

	# Set up one LDT
ldt_desc_ptr:
	.quad 0

gdt_bottom:

	.align 16
ldt:
	.rept 4
	.quad 0
	.endr
ldt_bottom:

.align 4
	.word 0 # Padding
idt_desc_ptr:
	.word idt_bottom - idt - 1
	.long idt


	.align  16
idt:
_idt:
	.rept NUM_VEC
	.quad 0
	.endr

idt_bottom:


/*Exception Handler Assembly Functions to save registers before and restore after calling
the high end C functions*/

divide_error:				# IDT 0
    pushal            		# push all registers
    call do_divide_error	# call divide error exception handler
    popal             		# restore all registers
    iret             		# and return from exception (may not happen if the c function terminates the process and doesn't return)

debug:						# IDT 1
    pushal            		# push all registers
    call do_debug			# call debug handler
    popal             		# restore all registers
    iret             		# and return from exception (may not happen if the c function terminates the process and doesn't return)

nmi:						# IDT 2
    pushal            		# push all registers
    call do_nmi				# call divide error exception handler
    popal             		# restore all registers
    iret             		# and return from exception (may not happen if the c function terminates the process and doesn't return)

int3:						# IDT 3
    pushal            		# push all registers
    call do_int3			# call breakpoint handler
    popal             		# restore all registers
    iret             		# and return from exception (may not happen if the c function terminates the process and doesn't return)

overflow:					# IDT 4
    pushal            		# push all registers
    call do_overflow		# call overflow handler
    popal             		# restore all registers
    iret             		# and return from exception (may not happen if the c function terminates the process and doesn't return)

bounds:						# IDT 5
    pushal            		# push all registers
    call do_bounds			# call bounds check handler
    popal             		# restore all registers
    iret             		# and return from exception (may not happen if the c function terminates the process and doesn't return)

invalid_op:					# IDT 6
    pushal            		# push all registers
    call do_invalid_op		# call invalid opcode handler
    popal             		# restore all registers
    iret             		# and return from exception (may not happen if the c function terminates the process and doesn't return)

device_not_available:					# IDT 7
    pushal            					# push all registers
    call do_device_not_available		# call overflow handler
    popal             					# restore all registers
    iret             					# and return from exception (may not happen if the c function terminates the process and doesn't return)

doublefault_fn:				# IDT 8
    pushal            		# push all registers
    call do_doublefault_fn	# call double fault handler
    popal             		# restore all registers
  	add $4, %esp     		# remove error code
    iret             		# and return from exception (may not happen if the c function terminates the process and doesn't return)

coprocessor_segment_overrun:					# IDT 9
    pushal            							# push all registers
    call do_coprocessor_segment_overrun			# call coprocessor segment overrun handler
    popal             							# restore all registers
    iret             							# and return from exception (may not happen if the c function terminates the process and doesn't return)

invalid_TSS:				# IDT 10
    pushal            		# push all registers
    call do_invalid_TSS		# call invalid TSS handler
    popal             		# restore all registers
  	add $4, %esp     		# remove error code
    iret             		# and return from exception (may not happen if the c function terminates the process and doesn't return)

segment_not_present:				# IDT 11
    pushal            				# push all registers
    call do_segment_not_present		# call segment not present handler
    popal             				# restore all registers
  	add $4, %esp     				# remove error code
    iret             				# and return from exception (may not happen if the c function terminates the process and doesn't return)

stack_segment:				# IDT 12
    pushal            		# push all registers
    call do_stack_segment	# call stack segment handler
    popal             		# restore all registers
  	add $4, %esp     		# remove error code
    iret             		# and return from exception (may not happen if the c function terminates the process and doesn't return)

general_protection:					# IDT 13
    pushal            				# push all registers
    call do_general_protection		# call general protection handler
    popal             				# restore all registers
  	add $4, %esp     				# remove error code
    iret             				# and return from exception (may not happen if the c function terminates the process and doesn't return)

page_fault:					# IDT 14
    pushal            		# push all registers
    call do_page_fault		# call page fault handler
    popal             		# restore all registers
  	add $4, %esp     		# remove error code
    iret             		# and return from exception (may not happen if the c function terminates the process and doesn't return)

intel_reserved: 			# IDT 15
	# Do nothing

coprocessor_error:					# IDT 16
    pushal            				# push all registers
    call do_coprocessor_error		# call floating point error handler
    popal             				# restore all registers
    iret             				# and return from exception (may not happen if the c function terminates the process and doesn't return)

alignment_check:				# IDT 17
    pushal            			# push all registers
    call do_alignment_check		# call alignment check handler
    popal             			# restore all registers
    iret             			# and return from exception (may not happen if the c function terminates the process and doesn't return)

machine_check:				# IDT 18
    pushal            		# push all registers
    call do_machine_check	# call machine check handler
    popal             		# restore all registers
    iret             		# and return from exception (may not happen if the c function terminates the process and doesn't return)

simd_coprocessor_error:					# IDT 19
    pushal            					# push all registers
    call do_simd_coprocessor_error		# call SIMD floating point handler
    popal             					# restore all registers
    iret             					# and return from exception (may not happen if the c function terminates the process and doesn't return)

rtc_handler:
	sti									# Renable interrupts (They are disabled when reading IDT entry) 
	pushf								# Save flags with interrupts enabled
    pushal            					# push all registers
    call do_rtc_handler					# call RTC handler
    popal            					# restore all registers
	popf								# restore flags
    iret             					# and return from exception (may not happen if the c function terminates the process and doesn't return)
	
keyboard:
	#sti									# Renable interrupts (They are disabled when reading IDT entry) 
	pushf								# Save flags with interrupts enabled
	pushal            					# push all registers
    call do_keyboard					# call Keyboard handler
    popal            					# restore all registers
	popf								# restore flags
    iret             					# and return from exception (may not happen if the c function terminates the process and doesn't return)

pit_handler:
	#sti									# Renable interrupts (They are disabled when reading IDT entry) 
	cli
	pushf								# Save flags with interrupts enabled
	pushal            					# push all registers
    call do_pit							# call pit handler
break_label:
	pushl %eax							# push pid
	addl $4, %esp
    popal            					# restore all registers
	popf								# restore flags
	movl -40(%esp), %edx				# put return value in edx
	pushl %edx							# put pid on stack
	cmpl $0, %edx
	je dont_switch						# dont switch if no process open yet
	sti
	call switch_process					# switch to next process
dont_switch:
	sti
	iret             					# and return from exception (may not happen if the c function terminates the process and doesn't return)
	
#Temporary handlers

asm_test_interrupts:
    pushf
	pushal
	call test_interrupts
	popal
	popf
	iret
