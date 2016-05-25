#Assembly for system calls

#define ASM	1
#include "sys_call_handler.h"

.globl sys_call_handler

jump_table: .long do_halt, do_execute, do_read, do_write, do_open, do_close, do_getargs, do_vidmaps, do_set_handler, do_sigreturn

ret_val: .int -1	# Temporary storage for our return value for 
					# when we restore the value of %EAX following 
					# the function return and temporarily overwrite 
					# the return stored in %EAX
esp_val: .int 1		# Variable to store %ESP
ebp_val: .int 1		# Variable to store %EBP
eip_val: .int 1		# Variable to store %EIP

# system_call_handler
#	FUNCTION:		Called by INT 0x80. Determines which system call was called based
#					on the system_call_number stored in %EAX and acts accordingly.
#					It saves the necessary registers and flags, then calls a function 
#					that carries out the system call itself. It calls these functions
#					using a jump table and the system_call_number.
#					The functions that are called call another function (these other
#					functions are implemented in "sys_calls.c") and, upon return of these
#					functions, they restore registers and return to the process that made
#					the system call.
#	INTPUT:			%EAX contains the system_call_number
#					%EBX contains argument #1
# 					%ECX contains argument #2
# 					%EDX contains argument #3
#					Individual function calls determine what these arguments mean and may
#					not necessarily make use of these inputs.
#	OUTPUT:			None
#	RETURN VALUE:	The return value is stored in %EAX.
#					The meaning of this value depends on which system call was called upon.
#					Not all functions return a value, although others will return a value
#					containing some meaningful information (or -1 on failure).
#	SIDE EFFECTS:	Carries out a system call. Effects vary based on which system call is called.

sys_call_handler:
	pushal #save registers
	pushf  #save flags
	
	pushl %ds
	pushl %es

	movl $0x18, %esi
	movl %esi, %ds   #restore ds to kernel
	movl %esi, %es   #restore es to kernel
	
	pushl %edx
	pushl %ecx
	pushl %ebx
	
	decl %eax
	jmp *jump_table(,%eax,4)	# Calls the appropriate function based on the system_call_number

do_halt:
	call halt
	jmp end_sys_call	#Shouldn't Get Back Here

do_execute:
	call execute
	jmp end_sys_call
	
do_read:
	call read
	jmp end_sys_call

do_write:
	call write
	jmp end_sys_call

do_open:
	call open
	jmp end_sys_call

do_close: 
	call close
	jmp end_sys_call

do_getargs: 
	call getargs
	jmp end_sys_call

do_vidmaps: 
	call vidmap
	jmp end_sys_call

do_set_handler:
	movl $-1,%eax
	jmp end_sys_call
	
do_sigreturn:
	movl $-1,%eax
	jmp end_sys_call
	
	
end_sys_call:
	#restore registers
	movl %eax, ret_val
	
	addl $12, %esp
	
	popl %ds
	popl %es
	
	popf
	popal
	movl ret_val, %eax
	iret
