/* kernel.c - the C part of the kernel
 * vim:ts=4 noexpandtab
 */

#include "multiboot.h"
#include "x86_desc.h"
#include "lib.h"
#include "i8259.h"
#include "debug.h"
#include "keyboard.h"
#include "terminal.h"
#include "filesystem.h"
#include "rtc.h"
#include "pit.h"
#include "sys_calls.h"
#include "sys_call_handler.h"

/*Function used to setup IDT, see header and Understanding the Linux Kernel page 141
  for more details (although these are implemented slightly differently than the book due to the use of
  the SET_IDT_ENTRY function)*/
static void set_kernel_int_gate(uint8_t n, idt_desc_t * idt);
static void set_user_int_gate(uint8_t n, idt_desc_t * idt);
static void init_IDT();

unsigned int page_directory[1024] __attribute__((aligned (4096)));
unsigned int first_page_table[1024] __attribute__((aligned (4096)));
unsigned int video_page_table[1024] __attribute__((aligned (4096)));

/* Macros. */
/* Check if the bit BIT in FLAGS is set. */
#define CHECK_FLAG(flags,bit)   ((flags) & (1 << (bit)))

/* Check if MAGIC is valid and print the Multiboot information structure
   pointed by ADDR. */
void
entry (unsigned long magic, unsigned long addr)
{
	multiboot_info_t *mbi;

	/* Clear the screen. */
	clear();

	/* Am I booted by a Multiboot-compliant boot loader? */
	if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
	{
		printf ("Invalid magic number: 0x%#x\n", (unsigned) magic);
		return;
	}

	/* Set MBI to the address of the Multiboot information structure. */
	mbi = (multiboot_info_t *) addr;

	/* Print out the flags. */
	printf ("flags = 0x%#x\n", (unsigned) mbi->flags);

	/* Are mem_* valid? */
	if (CHECK_FLAG (mbi->flags, 0))
		printf ("mem_lower = %uKB, mem_upper = %uKB\n",
				(unsigned) mbi->mem_lower, (unsigned) mbi->mem_upper);

	/* Is boot_device valid? */
	if (CHECK_FLAG (mbi->flags, 1))
		printf ("boot_device = 0x%#x\n", (unsigned) mbi->boot_device);

	/* Is the command line passed? */
	if (CHECK_FLAG (mbi->flags, 2))
		printf ("cmdline = %s\n", (char *) mbi->cmdline);

	if (CHECK_FLAG (mbi->flags, 3)) {
		int mod_count = 0;
		int i;
		module_t* mod = (module_t*)mbi->mods_addr;
		while(mod_count < mbi->mods_count) {
			printf("Module %d loaded at address: 0x%#x\n", mod_count, (unsigned int)mod->mod_start);
			printf("Module %d ends at address: 0x%#x\n", mod_count, (unsigned int)mod->mod_end);
			printf("First few bytes of module:\n");
			for(i = 0; i<16; i++) {
				printf("0x%x ", *((char*)(mod->mod_start+i)));
			}
			printf("\n");
			mod_count++;
		}
	}
	/* Bits 4 and 5 are mutually exclusive! */
	if (CHECK_FLAG (mbi->flags, 4) && CHECK_FLAG (mbi->flags, 5))
	{
		printf ("Both bits 4 and 5 are set.\n");
		return;
	}

	/* Is the section header table of ELF valid? */
	if (CHECK_FLAG (mbi->flags, 5))
	{
		elf_section_header_table_t *elf_sec = &(mbi->elf_sec);

		printf ("elf_sec: num = %u, size = 0x%#x,"
				" addr = 0x%#x, shndx = 0x%#x\n",
				(unsigned) elf_sec->num, (unsigned) elf_sec->size,
				(unsigned) elf_sec->addr, (unsigned) elf_sec->shndx);
	}

	/* Are mmap_* valid? */
	if (CHECK_FLAG (mbi->flags, 6))
	{
		memory_map_t *mmap;

		printf ("mmap_addr = 0x%#x, mmap_length = 0x%x\n",
				(unsigned) mbi->mmap_addr, (unsigned) mbi->mmap_length);
		for (mmap = (memory_map_t *) mbi->mmap_addr;
				(unsigned long) mmap < mbi->mmap_addr + mbi->mmap_length;
				mmap = (memory_map_t *) ((unsigned long) mmap
					+ mmap->size + sizeof (mmap->size)))
			printf (" size = 0x%x,     base_addr = 0x%#x%#x\n"
					"     type = 0x%x,  length    = 0x%#x%#x\n",
					(unsigned) mmap->size,
					(unsigned) mmap->base_addr_high,
					(unsigned) mmap->base_addr_low,
					(unsigned) mmap->type,
					(unsigned) mmap->length_high,
					(unsigned) mmap->length_low);
	}

	/* Construct an LDT entry in the GDT */
	{
		seg_desc_t the_ldt_desc;
		the_ldt_desc.granularity    = 0;
		the_ldt_desc.opsize         = 1;
		the_ldt_desc.reserved       = 0;
		the_ldt_desc.avail          = 0;
		the_ldt_desc.present        = 1;
		the_ldt_desc.dpl            = 0x0;
		the_ldt_desc.sys            = 0;
		the_ldt_desc.type           = 0x2;

		SET_LDT_PARAMS(the_ldt_desc, &ldt, ldt_size);
		ldt_desc_ptr = the_ldt_desc;
		lldt(KERNEL_LDT);
	}

	/* Construct a TSS entry in the GDT */
	{
		seg_desc_t the_tss_desc;
		the_tss_desc.granularity    = 0;
		the_tss_desc.opsize         = 0;
		the_tss_desc.reserved       = 0;
		the_tss_desc.avail          = 0;
		the_tss_desc.seg_lim_19_16  = TSS_SIZE & 0x000F0000;
		the_tss_desc.present        = 1;
		the_tss_desc.dpl            = 0x0;
		the_tss_desc.sys            = 0;
		the_tss_desc.type           = 0x9;
		the_tss_desc.seg_lim_15_00  = TSS_SIZE & 0x0000FFFF;

		SET_TSS_PARAMS(the_tss_desc, &tss, tss_size);

		tss_desc_ptr = the_tss_desc;

		tss.ldt_segment_selector = KERNEL_LDT;
		tss.ss0 = KERNEL_DS;
		tss.esp0 = 0x800000;  //128MB virtual for first process
		ltr(KERNEL_TSS);
	}

	/*Construct differnt IDT entries, first function called sets up the descriptor values in the idt array
	  then the second call sets up the handler for that value in the IDT*/
	init_IDT();
	/*Load the IDT*/
	lidt(idt_desc_ptr);

	/* Init the PIC */
	i8259_init();

	/* Initialize devices, memory, filesystem, enable device interrupts on the
	 * PIC, any other initialization stuff... */
	terminal_init();
	init_keyboard();
	module_t* mod = (module_t*)mbi->mods_addr;
	fs_init(mod->mod_start);
	update_video_page_pointer(video_page_table);
	
	//Enable PIT interrupts
	enable_irq(0);
	//Enable Keyboard Interrupts
	enable_irq(1);
	//Enable SLAVE PIC Interrupts
	enable_irq(2);
	//Enable Real Time Clock Interrupts
	enable_irq(8);

	/* Setup Paging  (help from wiki.osdev.org)*/

	//set each entry to not present
	int i = 0;
	
	//start at memory 0 and go til 4MB
	unsigned int address = 0;
	
	for(i = 0; i < 1024; i++)
	{
		//attribute: supervisor level, read/write, not present.
		page_directory[i] = address | 0x87;
		address += 0x400000;
	}
	
	address = 0;

	//we will fill all 1024 entries, mapping first 4 megabytes
	for(i = 0; i < 1024; i++)
	{
		first_page_table[i] = address | 3; // attributes: supervisor level, read/write, present.
		video_page_table[i] = 0;
		address = address + 4096;   //advance the address to the next page boundary
	}

	first_page_table[0] = 0;

	//put page tables in page directory
	page_directory[0] = (unsigned int)first_page_table;
	page_directory[0] |= 3;   //attributes: supervisor level, read/write, present
	
	/*do the same for kernel mapping to 4MB to 8MB */
	/*	set the page Base Adress bits 31-22        */
	/*	set as a global page, bit 8                */
	/*	set as as a 4MB page, bit 7                */
	/*	set as supervisor, bit 2 = 0               */
	/*	set as read/write, bit 1                   */
	/*	set as present, bit 0                      */
	page_directory[1] = 0x00400183;
	
	// 4MB user program page at 8MB physical / 128MB virtual
	page_directory[32] = 0x00800087;
	
	// 4kB video memory at 0xB8000 physical / 256MB virtual
	video_page_table[0] = 0xB8000 | 7;
	page_directory[64] = (unsigned int)video_page_table;
	page_directory[64] |= 7;
	
	/* Enable Paging (help from wiki.osdev.org) */

	//moves page_directory into the cr3 register.
	asm volatile("mov %0, %%cr3":: "b"(page_directory));

	//set mix bits for CR4
	unsigned int cr4;
	asm volatile("mov %%cr4, %0": "=b"(cr4));
	cr4 |= 0x00000010;
	asm volatile("mov %0, %%cr4":: "b"(cr4));

	//sets paging enable bit of cr0
	unsigned int cr0;
	asm volatile("mov %%cr0, %0": "=b"(cr0));
	cr0 |= 0x80000000;
	asm volatile("mov %0, %%cr0":: "b"(cr0));

	/* Enable interrupts */
	/* Do not enable the following until after you have set up your
	 * IDT correctly otherwise QEMU will triple fault and simple close
	 * without showing you any output */
	//printf("Enabling Interrupts\n");
	sti();

	/* Execute the first program (`shell') ... */

	sys_call_pd_addrs(page_directory);

	clear();
	set_CRTC_registers();
	update_cursor(0, 7);
	switch_terminal(1);

	/* Spin (nicely, so we don't chew up cycles) */
	asm volatile(".1: hlt; jmp .1;");
}

/*
 * set_kernel_int_gate
 *   DESCRIPTION: Sets up a Interrupt Gate entry in the IDT according to the format specified below,
 *      should be followed by a call to SET_IDT_ENTRY to setup the handler as well.
 *   INPUTS: n - the entry in the idt array to be modified, idt - the IDT table (stored as an array of 256 idt_desc_t's)
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: Modifies an IDT entry
 *
 *   FORMAT:
 *      -seg_selector: Kernel Code Segment
 *      -reserved4: 0
 *      -reserved3: 0
 *      -reserved2: 1
 *      -reserved1: 0
 *      -reserved0: 0
 *      -size: 1
 *      -dpl: 0 (privileged/kernel level access)
 *      -present: 1
 *
 */
static void set_kernel_int_gate(uint8_t n, idt_desc_t * idt)
{

	idt[n].seg_selector = KERNEL_CS;

	idt[n].reserved4 = 0x00;
	idt[n].reserved3 = 0;
	idt[n].reserved2 = 1;
	idt[n].reserved1 = 1;
	idt[n].size = 1;
	idt[n].reserved0 = 0;
	idt[n].dpl = 0x00;
	idt[n].present = 1;

	return;
}

/*
 * set_user_int_gate
 *   DESCRIPTION: Sets up a Interrupt Gate entry in the IDT according to the format specified below,
 *      should be followed by a call to SET_IDT_ENTRY to setup the handler as well.
 *   INPUTS: n - the entry in the idt array to be modified, idt - the IDT table (stored as an array of 256 idt_desc_t's)
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: Modifies an IDT entry
 *
 *   FORMAT:
 *      -seg_selector: Kernel Code Segment
 *      -reserved4: 0
 *      -reserved3: 0
 *      -reserved2: 1
 *      -reserved1: 0
 *      -reserved0: 0
 *      -size: 1
 *      -dpl: 3 (User level access)
 *      -present: 1
 *
 */
static void set_user_int_gate(uint8_t n, idt_desc_t * idt)
{
	/*
	 * Everything is set up according to the format.
	 * More information on the format can be found in
     * pages 2-3 of the Descripter document (which can,
	 * in turn, be found in the Tools, References and
	 * Links section in the ECE 391 website.
	 */

	idt[n].seg_selector = KERNEL_CS;

	idt[n].reserved4 = 0x00;
	idt[n].reserved3 = 0;
	idt[n].reserved2 = 1;
	idt[n].reserved1 = 1;
	idt[n].reserved0 = 0;

	idt[n].size = 1;
	idt[n].dpl = 3;
	idt[n].present = 1;

	return;
}

/*
 * do_divide_error
 *	PURPOSE: This is the exception handler for division errors, namely division by zero (which results in some undefined value).
 *	INPUT: none
 *	OUTPUT: Prints information and terminates user program.
 *	RETURN VALUE: none
 *	SIDE EFFECTS: While we are still testing, it might give some meaningful print statements to help in debugging. Ultimately, it
 * 		will terminate the user code if a division error occurs.
 */
void do_divide_error()
{
	sti();
    printf("You divided by zero and broke the universe!! Now you must die >:D\n");
	asm volatile(".2: hlt; jmp .2;");
}

/*
 * do_debug
 *	PURPOSE: Exception handler
 *	INPUT: none
 *	OUTPUT: While we are still debugging, this handler might print information
 *	RETURN VALUE: none
 *	SIDE EFFECTS: terminates user code
 */
void do_debug()
{
	printf("Hi. This is your debug handler telling you that your code is infested with bugs.\n");
	asm volatile(".3: hlt; jmp .3;");
}

/*
 * do_nmi
 *	PURPOSE: Exception handler for non-maskeable-interrupts
 *	INPUT: None
 *	OUTPUT:
 *	RETURN VALUE: None
 *	SIDE EFFECTS: Gives control over to the interrupt
 */
extern void do_nmi()
{
	printf("nmi coming through.\n");
	asm volatile(".4: hlt; jmp .4;");
}

/* do_int3
 *	PURPOSE: This is the breakpoint exception handler.
 *	INPUT: None
 *	OUTPUT: Prints which error occured to screen.
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None
 */
void do_int3()
{
	printf("BREAKPOINT!\n");
	asm volatile(".5: hlt; jmp .5;");
}

/*
 * do_overflow
 *	PURPOSE: This is the overflow exception handler. It occurs when you get an overflow.
 *	INPUT: None
 *	OUTPUT: Prints meaningful information to the screen. This is used primarily for debugging purposes and will
 *  	change once it is properly implemented.
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None.
 */
void do_overflow()
{
	printf("Overflow. ITS OVER 4294967295!!!! (also over 9000)\n");
	asm volatile(".6: hlt; jmp .6;");
}

/*
 * do_bounds
 *	PURPOSE: This is the boundary check exception handler.
 *	INPUT: None
 *	OUTPUT: Prints meaningful information to the screen. This is used primarily for debugging purposes and will
 *  	change once it is properly implemented.
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None.
 */
void do_bounds()
{
	printf("Boundary exception. I have no idea what it does.\n");
	asm volatile(".7: hlt; jmp .7;");
}

/*
 * do_invalid_op
 *	PURPOSE: This is the invalid opcode exception handler. It occurs when an invalid opcode is called.
 *	INPUT: None
 *	OUTPUT: Prints meaningful information to the screen. This is used primarily for debugging purposes and will
 *  	change once it is properly implemented.
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None.
 */
void do_invalid_op()
{
	printf("Invalid opcode.\n");
	asm volatile(".9: hlt; jmp .9;");
}

/*
 * do_device_not_available
 *	PURPOSE: This is the device unavailable exception handler. It occurs when you try to access a device that isn't there.
 *	INPUT: None
 *	OUTPUT: Prints meaningful information to the screen. This is used primarily for debugging purposes and will
 *  	change once it is properly implemented.
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None.
 */
void do_device_not_available()
{
	printf("Maybe the device is just a ghost and only you can see it? Sorry, the device is not available.\n");
	asm volatile(".10: hlt; jmp .10;");
}

/*
 * do_doublefault_fn
 *	PURPOSE: This is the double fault exception handler.
 *	INPUT: None
 *	OUTPUT: Prints meaningful information to the screen. This is used primarily for debugging purposes and will
 *  	change once it is properly implemented.
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None.
 */
void do_doublefault_fn()
{
	printf("A fault within a fault? Faultception! (Or just a regular doublefault. Whatever.) \n");
	asm volatile(".11: hlt; jmp .11;");
}

/*
 * do_coprocessor_segment_overrun
 *	PURPOSE: This is the coprocessor segment overrun exception handler.
 *	INPUT: None
 *	OUTPUT: Prints meaningful information to the screen. This is used primarily for debugging purposes and will
 *  	change once it is properly implemented.
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None.
 */
void do_coprocessor_segment_overrun()
{
	printf("Coprocessor segment overrun...\n");
	asm volatile(".12: hlt; jmp .12;");
}

/*
 * do_invalid_TSS()
 *	PURPOSE: This is the invalid TSS exception handler.
 *	INPUT: None
 *	OUTPUT: Prints meaningful information to the screen. This is used primarily for debugging purposes and will
 *  	change once it is properly implemented.
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None.
 */
void do_invalid_TSS()
{
	printf("Invalid TSS.\n");
	asm volatile(".13: hlt; jmp .13;");
}

/*
 * do_segment_not_present()
 *	PURPOSE: This is the segment not present exception handler.
 *	INPUT: None
 *	OUTPUT: Prints meaningful information to the screen. This is used primarily for debugging purposes and will
 *  	change once it is properly implemented.
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None.
 */
void do_segment_not_present()
{
	printf("Segment not present. Where did you put it?\n");
	asm volatile(".14: hlt; jmp .14;");
}

/*
 * do_stack_segment
 *	PURPOSE: This is the stack segment fault exception handler.
 *	INPUT: None
 *	OUTPUT: Prints meaningful information to the screen. This is used primarily for debugging purposes and will
 *  	change once it is properly implemented.
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None.
 */
void do_stack_segment()
{
	printf("Stack segment fault. Darn segfaults.\n");
	asm volatile(".15: hlt; jmp .15;");
}

/*
 * do_general_protection()
 *	PURPOSE: This is the general protection exception handler.
 *	INPUT: None
 *	OUTPUT: Prints meaningful information to the screen. This is used primarily for debugging purposes and will
 *  	change once it is properly implemented.
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None.
 */
void do_general_protection()
{
	printf("General protection. Again, no clue as to what it is.\n");
	asm volatile(".16: hlt; jmp .16;");
}

/*
 * do_page_fault()
 *	PURPOSE: This is the page fault exception handler.
 *	INPUT: None
 *	OUTPUT: Prints meaningful information to the screen. This is used primarily for debugging purposes and will
 *  	change once it is properly implemented.
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None.
 */
void do_page_fault()
{
	uint32_t fault_addr = 0x0;
	asm volatile("movl %%cr2, %0"
		:"=r"(fault_addr)
		:
	);
	printf("Page fault addrs: %x.\n", fault_addr);
	asm volatile(".17: hlt; jmp .17;");
}

/*
 * do_coprocessor_error
 *	PURPOSE: This is the floating point error exception handler.
 *	INPUT: None
 *	OUTPUT: Prints meaningful information to the screen. This is used primarily for debugging purposes and will
 *  	change once it is properly implemented.
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None.
 */
void do_coprocessor_error()
{
	printf("You have erroneous points just floating about.\n");
	asm volatile(".18: hlt; jmp .18;");
}

/*
 * do_alignment_check
 *	PURPOSE: This is the alignment check exception handler.
 *	INPUT: None
 *	OUTPUT: Prints meaningful information to the screen. This is used primarily for debugging purposes and will
 *  	change once it is properly implemented.
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None.
 */
void do_alignment_check()
{
	printf("Things are not aligned.\n");
	asm volatile(".19: hlt; jmp .19;");
}

/*
 * do_machine_check
 *	PURPOSE: This is the machine check exception handler.
 *	INPUT: None
 *	OUTPUT: Prints meaningful information to the screen. This is used primarily for debugging purposes and will
 *  	change once it is properly implemented.
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None.
 */
void do_machine_check()
{
	printf("Machine check.\n");
	asm volatile(".20: hlt; jmp .20;");
}

/*
 * do_simd_coprocessor_error
 *	PURPOSE: This is the SIMD floating point error exception handler.
 *	INPUT: None
 *	OUTPUT: Prints meaningful information to the screen. This is used primarily for debugging purposes and will
 *  	change once it is properly implemented.
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None.
 */
void do_simd_coprocessor_error()
{
	printf("Why are you using those evil vector registers? They're evil. Also, they gave you a SIMD floating point error. I hope that stuff's not contagious.\n");
	asm volatile(".21: hlt; jmp .21;");
}

/*
 * do_rtc_handler
 *	PURPOSE: Handle an Real Time Clock interrupt
 *	INPUT: None
 *	OUTPUT: None
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None
 */
void do_rtc_handler()
{
	outb(0x0C, 0x70);
	inb(0x71);
	tick();
	send_eoi(8);
	return;
}

/*
 * do_pit
 *	PURPOSE: Handle a PIT interrupt
 *	INPUT: None
 *	OUTPUT: None
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None
 */
int do_pit()
{
	send_eoi(0);
	
	store_state();
	
	//write low byte to channel 0 (0x40)
	outb(0x00, 0x40);
	
	//write high byte to channel 0 (0x40)
	outb(0x00, 0x40);
	
	//jump to next process
	int pid = get_next_process();
	
	return pid;
}

/*
 * switch_process
 *	FUNTION:		Calls jump to process with the same parameters.
 *	INPUT:			int pid - Process ID
 *	OUTPUT:			None
 *	RETURN VALUE:	None
 *	SIDE EFFECT:	Switches processes.
 */
void switch_process(int pid){
	jump_to_process(pid);
}

/*
 * do_keyboard
 *	PURPOSE: Keyboard interrupt handler
 *	INPUT: None
 *	OUTPUT: print scancode of input from keyboard to screen
 *	RETURN VALUE: None
 *	SIDE EFFECTS: None.
 */void do_keyboard()
{
	//schedule_active_terminal();
	cli();
	uint32_t input = inb(0x60);
	print_scancode(input);
	//return_to_terminal();
	send_eoi(1);
	sti();
	return;
}

/*
 * init_IDT
 *	FUNCTION:		Initializes the IDT
 *	INPUT:			None
 *	OUTPUT:			None
 *	RETURN VALUE:	None
 *	SIDE EFFECTS:	Associates certain IDT entries with interrupt handlers
 */
void init_IDT()
{
/******EXCEPTION IDT ENTRIES**************************/
	//Entry for Divide_Error
	set_kernel_int_gate(0, idt);
	SET_IDT_ENTRY(idt[0], divide_error);
	//Entry for Debug
	set_kernel_int_gate(1, idt);
	SET_IDT_ENTRY(idt[1], debug);
	//Entry for NMI
	set_kernel_int_gate(2, idt);
	SET_IDT_ENTRY(idt[2], nmi);
	//Entry for Breakpoint
	set_kernel_int_gate(3, idt);
	SET_IDT_ENTRY(idt[3], int3);
	//Entry for Overflow
    set_kernel_int_gate(4, idt);
    SET_IDT_ENTRY(idt[4], overflow);
	//Entry for Bounds Check
	set_kernel_int_gate(5, idt);
	SET_IDT_ENTRY(idt[5], bounds);
	//Entry for Invalid Opcode
	set_kernel_int_gate(6, idt);
	SET_IDT_ENTRY(idt[6], invalid_op);
	//Entry for Device not Available
	set_kernel_int_gate(7, idt);
	SET_IDT_ENTRY(idt[7], device_not_available);
	//Entry for Double fault
	set_kernel_int_gate(8, idt);
	SET_IDT_ENTRY(idt[8], doublefault_fn);
	//Entry for Coprocessor segment overrun
	set_kernel_int_gate(9, idt);
	SET_IDT_ENTRY(idt[9], coprocessor_segment_overrun);
	//Entry for invalid_TSS
	set_kernel_int_gate(10, idt);
	SET_IDT_ENTRY(idt[10], invalid_TSS);
	//Entry for segment_not_present
	set_kernel_int_gate(11, idt);
	SET_IDT_ENTRY(idt[11], segment_not_present);
	//Entry for stack_segment
	set_kernel_int_gate(12, idt);
	SET_IDT_ENTRY(idt[12], stack_segment);
	//Entry for general_protection
	set_kernel_int_gate(13, idt);
	SET_IDT_ENTRY(idt[13], general_protection);
	//Entry for page_fault
    set_kernel_int_gate(14, idt);
    SET_IDT_ENTRY(idt[14], page_fault);
	//Intel Reserved Number 15
	//Entry for coprocessor_error
	set_kernel_int_gate(16, idt);
	SET_IDT_ENTRY(idt[16], coprocessor_error);
	//Entry for alignment_check
	set_kernel_int_gate(17, idt);
	SET_IDT_ENTRY(idt[17], alignment_check);
	//Entry for machine_check
	set_kernel_int_gate(18, idt);
	SET_IDT_ENTRY(idt[18], machine_check);
	//Entry for simd_coprocessor_error
	set_kernel_int_gate(19, idt);
	SET_IDT_ENTRY(idt[19], simd_coprocessor_error);


/******Interrupt IDT ENTRIES**************************/
	//Entry for Timer
	set_kernel_int_gate(0x20, idt);
	SET_IDT_ENTRY(idt[0x20], pit_handler);
	//Entry for Keyboard interrupt
	set_kernel_int_gate(0x21, idt);
	SET_IDT_ENTRY(idt[0x21], keyboard);
	//Entry for Real Time Clock
	set_kernel_int_gate(0x28, idt);
	SET_IDT_ENTRY(idt[0x28], rtc_handler);


/******SYS Call Entries******************************/
	set_user_int_gate(0x80, idt);
	SET_IDT_ENTRY(idt[0x80], sys_call_handler);

}
