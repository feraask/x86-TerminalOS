/*
	C source for system calls.
*/

#ifndef _SYS_CALLS_C
#define _SYS_CALLS_C

#include "sys_calls.h"
#include "lib.h"
#include "filesystem.h"
#include "rtc.h"
#include "terminal.h"
#include "directory.h"


//local pointers to important memory locations
unsigned int * page_dir;
pcb_t * current_pcb = 0x0;
file_t * file_array = 0x0;
char * used_desc = 0x0;
int num_process = 1;
int open_pid[6] = {0,0,0,0,0,0};
int open_terminals[4] = {0,0,0,0};
int active_process[4] = {0,0,0,0}; 
int active_terminals[4] = {0,0,0,0};
int cur_terminal = 0;
int cur_process = 1;
int sched_on = 0;
uint32_t* t_esp;
unsigned int * video_pg_table;


/*
 * halt
 * FUNCTION: 	 System call that terminates caller process and 
 *				 returns control to the parent process.
 * INTPUT: 		 uint8_t status
 * OUTPUT: 		 None
 * RETURN VALUE: Returns status to the parent process.
 *				 Nothing is returned to the caller process as it
 *				 terminated in the function.
 *				 Returns -1 on Failure.
 * SIDE EFFECTS: Halts the process that calls it.
 */
int32_t halt (uint8_t status){
	cli();

	if(current_pcb -> pid == open_terminals[current_pcb -> terminal_id]){ 
		open_terminals[current_pcb -> terminal_id] = 0;	
		active_terminals[current_pcb -> terminal_id] = 0;
		active_process[current_pcb -> terminal_id] = 0;
		open_pid[(current_pcb -> pid)-1] = 0;   //free pid for another process's use
		clear_video_mem(current_pcb -> pid);
		clear_pcb(current_pcb -> pid);
		num_process--;
	
		int pos;
		for(pos = 1; pos < 4; pos++){
			if(open_terminals[pos] != 0){
				switch_terminal(pos);
			}
		}
		printf("System Halted");
		while(1){
			//Spin forever
		}		
	}
	
	//Change Paging back to parent process
	page_dir[32] = (0x800000 + (0x400000 * (current_pcb -> parent_pid -1))) | 0x87; //0x00800087;
	//Clear Buffer
	asm volatile("mov %0, %%cr3":: "b"(page_dir));
	tss.esp0 = current_pcb -> parent_esp;   //((uint32_t *)current_pcb)[5];//(uint32_t)((uint8_t *)parent_process + 8192);
	
	//Push ebp,esp(both are done in execute), and status(the return value)
	//    then jmp to the label at the end of execute
	int32_t retval = status & 0x000000FF;
	uint32_t esp = current_pcb -> k_esp;   //((uint32_t *)current_pcb)[3];
	uint32_t ebp = current_pcb -> k_ebp;   //((uint32_t *)current_pcb)[4];
	
	num_process--;
	update_screen_x_y(current_pcb);
	update_parent_video(current_pcb);
	active_process[current_pcb -> terminal_id] = current_pcb -> parent_pid;
	open_pid[(current_pcb -> pid)-1] = 0;   //free pid for another process's use
	clear_pcb(current_pcb -> pid);
	
	update_cur_pcb(current_pcb -> parent_pcb);
	update_pointers(current_pcb, 1);
	
	asm volatile("movl %1, %%esp\n\t"
				"movl %2, %%ebp\n\t"
				"pushl %0\n\t"
				"jmp finishHalt\n\t"
				:
				:"r"(retval), "r"(esp), "r"(ebp)
	);
	
	return -1;
}

/*
 * execute
 *	FUNCTION:	  Executes given command in a new process if it is a valid executable.
 *				  Switches to user mode to execute command.
 *	INTPUT: 	  uint8_t * command - String command to run, including space separated 
 *				 					  arguments
 *	OUTPUT: 	  None
 *	RETURN VALUE: Returns the status when process is ended
 *	SIDE EFFECTS: Jumps to user mode
 */
int32_t execute (const uint8_t* command){
	cli();
	if(num_process > 6){
		printf("No more processes can be run.\n");
		return 0;
	}
	
	uint32_t k_esp, esp, eip, k_ebp, ss, cs, ds;

	//local variable to hold command
	int8_t com[strlen((int8_t *)command)];
	strcpy(com, (int8_t *)command);
	
	//Split command and arguments
	int8_t args[strlen(com)];
	int i;
	int argsStarted = 0;
	int len = strlen(com) + 1;
	for (i = 0; i < len; i++)
	{
		if(com[i] == ' ' && argsStarted == 0)
		{
			argsStarted = i + 1;
			com[i] = '\0';
		}
			
		else if(argsStarted != 0)
		{
			args[i-argsStarted] = com[i];
		}
		
	}
	if (argsStarted == 0)
	{
		//No arguments
		args[0] = '\0';
	}
	
	//only load if program is valid and executable
	dentry_t executable;
	if(is_valid_cmd(&executable, (uint8_t *)com)== -1){
		return -1;
	}

	int pid_pos=0;
	while(open_pid[pid_pos] == 1){
		pid_pos++;
	}
	open_pid[pid_pos] = 1;  //pid is now taken by new process
	pid_pos++;              //new process's pid is pid_pos + 1
	
	//test if this is not first process, need to update page directory
	if(current_pcb != 0x0)
	{
		page_dir[32] = (0x800000 + (0x400000 * (pid_pos-1))) | 0x87;  //0x00C00087;
		asm volatile("mov %0, %%cr3":: "b"(page_dir));
	}
	
	//Load Program into memory
	pcb_t * pcb = load_program((uint8_t *)com, &esp, &eip, pid_pos);
	if (pcb == NULL)
	{
		//Max tasks reached or no such command
		open_pid[--pid_pos] = 0;
		return -1;
	}
	
	pcb -> terminal_id = cur_terminal;
	
	if(open_terminals[cur_terminal] == 0){
		open_terminals[cur_terminal] = pcb -> pid;
	}
	
	active_process[cur_terminal] = pcb -> pid;
	copy_video_mem_in(cur_terminal);
	write_terminal_number(pcb -> terminal_id);
	num_process++;
	
	if(current_pcb != NULL){
		pcb -> parent_pid = current_pcb -> pid;
		pcb -> parent_pcb = current_pcb;
	}
	else{
		pcb -> parent_pid = 0;
		pcb -> parent_pcb = 0x0;
	}
	
	update_cur_pcb(pcb);
	
	//update virtual rtc
	change_to_virtual_rtc(pcb->pid);

	//stdin
	pcb -> file_array[0].f_ops.open = &terminal_open;
	pcb -> file_array[0].f_ops.read = &terminal_read;
	pcb -> file_array[0].f_ops.write = NULL;
	pcb -> used_desc[0] = 1;
	
	//std out
	pcb -> file_array[1].f_ops.read = NULL;
	pcb -> file_array[1].f_ops.write = &terminal_write;
	pcb -> used_desc[1] = 1;
	
	//clear remaining file array positions
	int pos;
	for(pos=2; pos<8; pos++){
		pcb -> used_desc[pos] = 0;
	}
	
	//Save esp and ebp for halt
	asm volatile("movl %%ebp, %0\n\t"
				 "movl %%esp, %1\n\t"
				:"=r"(k_ebp), "=r"(k_esp)
	);
	
	pcb -> k_esp = k_esp;   //current kernel level stack for halt
	pcb -> k_ebp = k_ebp;   //current kernel level base for halt
	
	//save parent esp
	pcb -> parent_esp = tss.esp0;
	
	//Save args
	strcpy(pcb -> args, args);

	//update video pointers
	update_pointers(pcb, -1);
	
	ss = USER_DS;
	cs = USER_CS;
	ds = USER_DS;
	
	if(cur_terminal == 1 && num_process == 2 && sched_on == 0){
		start_pit();
		sched_on = 1;
	}
	
	//update tss
	tss.esp0 = (uint32_t)((uint8_t *)pcb + 8192);
	tss.ss0 = KERNEL_DS;
	
	int flag;
	asm volatile("pushfl\n\t"
				 "popl %%edx\n\t"
				 "orl $0x200, %%edx\n\t"
				 "movl %%edx, %0\n\t"
				 :"=r"(flag)
	);
	
	//execute program
	asm volatile("pushl %0\n\t"
				"pushl %1\n\t"
				"pushl %5\n\t"
				//"pushfl\n\t"
				//"popl %%edx\n\t"
				//"orl $0x200, %%edx\n\t"
				//"pushl %%edx\n\t"
				"pushl %2\n\t"
				"pushl %3\n\t"
				"movl %4, %%ds\n\t"
				"movl %4, %%es\n\t"
				"movl %1, %%ebp\n\t"
				"iret"
				: 
				: "r" (ss), "r"(esp), "r"(cs), "r"(eip), "r"(ds), "r"(flag)
	);
	
	//Return from halt
	int32_t retval = -1;
	asm volatile("finishHalt: popl %0\n\t"
				:"=r"(retval)
	);
	
	return retval;
}

/*
 * read
 *	FUNCTION:		Executes file's specific read driver function
 *	INTPUT: 		fd     - file descriptor to file to open
 *					buf    - buffer to read from
 *					nbytes - number of bytes to read 
 *	OUTPUT: 		None
 *	RETURN VALUE: 	Returns the value of driver specific function
 *	SIDE EFFECTS:	Fills the buffer that was passed in
 */
int32_t read (int32_t fd, void* buf, int32_t nbytes){
	//bad file descriptor
	if(fd < 0 || fd > 7){
		return -1;
	}
	//regular file, need special buffer setup for fs_read()
	if(file_array[fd].f_dentry.file_type == 2){
		//check if file read
		if (file_array[fd].eof == 1)
		{
			return 0;
		}
		
		int bytes_read;
		
		if (nbytes < 12)
		{
			uint32_t buffer[3];
			buffer[0] = 1;
			buffer[1] = file_array[fd].f_dentry.inode_num;
			buffer[2] = file_array[fd].f_pos;
			bytes_read = file_array[fd].f_ops.read(buffer, nbytes);
			
			//refill buf
			int i;
			for (i = 0; i < nbytes; i++)
			{
				((uint8_t *)buf)[i] = ((uint8_t *)buffer)[i];
			}
		}
		else
		{
			((uint32_t *)buf)[0] = 1;
			((uint32_t *)buf)[1] = file_array[fd].f_dentry.inode_num;
			((uint32_t *)buf)[2] = file_array[fd].f_pos;
			bytes_read = file_array[fd].f_ops.read(buf, nbytes);
		}
		
		if (bytes_read != nbytes)
		{
			//end of file reached
			file_array[fd].eof = 1;
			if (file_array[fd].f_pos != 0)
			{
				return bytes_read - file_array[fd].f_pos;
			}
		}
		file_array[fd].f_pos += bytes_read;
		return bytes_read;
	}
	//rtc or directory
	return file_array[fd].f_ops.read(buf, nbytes);
}

/*
 * write
 *	FUNCTION: 		Executes file's specific write driver function
 *	INTPUT: 		fd     - file descriptor used to open file
 *		   			buf    - buffer to write to
 *		   			nbytes - number of bytes to write
 *	OUTPUT: 		buf is filled
 *	RETURN VALUE: 	return value of driver specific function
 */
int32_t write (int32_t fd, const void* buf, int32_t nbytes){
	//error checking for bad file descriptor or non-rtc file (write is only valid for rtc)
	if(fd < 0 || fd > 7)
	{
		return -1;
	}
	//call write
	return file_array[fd].f_ops.write(buf, nbytes);
}

/*
 * open
 *	FUNCTION: 		Creates a new file_t struct for a given file and assigns it a 
 *					new file descriptor.  Updates the file array and calls the file's 
 *					specific open driver function.
 *	INTPUT:			filename - string name of file to open
 *	OUTPUT:			used_desc and file array updated
 *	RETURN VALUE:	-1 on failure, integer with file descriptor on success
 */
int32_t open (const uint8_t* filename){
	int file_desc = 0;
	dentry_t dentry;
	int val = read_dentry_by_name(filename, &dentry);
	
	//bad filename
	if(val == -1){
		return -1;
	}

	//find empty file descriptor
	int pos;
	for(pos=2; pos < 8; pos++){
		if(used_desc[pos] != 1){
			file_desc = pos;
			break;
		}
	}

	//no empty file descriptors
	if(file_desc == 0){
		return -1;
	}

	file_t * new_file = file_array + file_desc;
	(*new_file).f_dentry = dentry;
	(*new_file).f_pos = 0;
	(*new_file).eof = 0;
	
	//RTC
	if(dentry.file_type == 0){
		(*new_file).f_ops.open = &rtc_open;
		(*new_file).f_ops.read = &rtc_read;
		(*new_file).f_ops.write = &rtc_write;
		//Call RTC Open
		rtc_open();
	}
	//Directory
	else if(dentry.file_type == 1){
		(*new_file).f_ops.open = &dir_open;
		(*new_file).f_ops.read = &dir_read;
		(*new_file).f_ops.write = &dir_write;
		dir_open();
	}
	//Regular file
	else{
		(*new_file).f_ops.open = &fs_open;
		(*new_file).f_ops.read = &fs_read;
		(*new_file).f_ops.write = &fs_write;
	}

	used_desc[file_desc] = 1;
	return file_desc;    //return file descriptor
}

/*
 * close
 *	FUNCTION:		Frees file descriptor to be used for another file and calls
 *					device driver specific close function if necessary
 * INTPUT: 			fd - integer file descriptor to close
 * OUTPUT: 			used_desc array updated
 * RETURN VALUE:	-1 on failure, 0 on success
 */
int32_t close (int32_t fd){
	//Check for bad file descriptor (you can't close 0, 1, or > 7)
	if(fd <= 1 || fd > 7)
		return -1;
	//If RTC file type, close the RTC
	if(file_array[fd].f_dentry.file_type == 0)
		rtc_close(fd);
	//Otherwise just open the used file array location at fd
	used_desc[fd] = 0x0;
	return 0;
}

/*
 * getargs
 * 	FUNCTION:	 	Reads the program’s command line arguments into a user-level buffer.
 *	INTPUT: 		uint8_t* buf   - Contains the buffer where we want to move data into
 *				 	int32_t nbytes - Contains number of bytes to be read 
 * 	OUTPUT:		 	None
 * 	RETURN VALUE:	None
 * 	SIDE EFFECTS:	Moves command line arguments in the memory starting at *buf
 */
int32_t getargs (uint8_t* buf, int32_t nbytes){
	if (nbytes < ARGS_MAX)
		return -1;
		
	strcpy((int8_t*)buf, current_pcb -> args );
	
	return 0;
}

/*
 * vidmap
 *	FUNCTION:		Enters the virtual address to the start of video memory
 *					into the pointer given.
 *	INTPUT: 		screen_start - pointer in user space to pointer pointer to 
 *					video memory
 *	OUTPUT: 		None
 *	RETURN VALUE: 	0 on success
 *	SIDE EFFECTS: 	None
 */
int32_t vidmap (uint8_t** screen_start){
	//test if pointer to fill is inside user space (128MB - 132MB)
	if(screen_start < (uint8_t **)0x08000000 || screen_start > (uint8_t **)0x08400000){
		return -1;
	}
	*screen_start = (uint8_t *)0x10000000;  //256 MB virtual address for video memory
	
	return 0;
}

/*
 * set_handler (NOT IMPLEMENTED / EXTRA CREDIT)
 * FUNCTION:
 * INTPUT: 
 * OUTPUT:
 * RETURN VALUE: -1
 */
int32_t set_handler (int32_t signum, void* handler_address){
	printf("inside set handler\n");
	return -1;
}

/*
 * sigreturn (NOT IMPLEMENTED / EXTRA CREDIT)
 * FUNCTION:
 * INTPUT: 
 * OUTPUT:
 * RETURN VALUE: -1
 */
int32_t sigreturn (void){
	printf("inside sig return\n");
	return -1;
}

/*
 * copy_video_mem_in
 *   DESCRIPTION:	Copies 4KB of process's video memory into virtual address 128MB
 *   INPUTS:		pid - process whose memory is to be copied to 128MB
 *   OUTPUTS: 		None
 *   RETURN VALUE: 	None
 *   SIDE EFFECTS:	Video memory that is displayed is updated
 */
void copy_video_mem_in(int terminal_id){
	//copy 4KB of process's video memory to virtual addrs 128MB
	//0x10000000 - 128 MB
	//0xB8000    - start of video memory
	//0x1000     - 4 KB
	memcpy((void *)0x10000000, (void *)(0xB8000+(terminal_id * 0x1000)), 0x1000);
}

/*
 * copy_video_mem_out
 *   DESCRIPTION: 	Copies 4KB from virtual address 128MB into address specific
 *					for given process
 *   INPUTS:		pid - process whose memory need to be written to
 *   OUTPUTS:		None
 *   RETURN VALUE:	None
 *   SIDE EFFECTS:	4KB video page belonging to process overwritten
 */
void copy_video_mem_out(int terminal_id){
	//copy 4KB of video memory to process's video memory
	//0x10000000 - 128 MB
	//0xB8000    - start of video memory
	//0x1000     - 4 KB
	memcpy((void *)(0xB8000+(terminal_id * 0x1000)), (void *)0x10000000, 0x1000);
}

/*
 * swap_video_pages 
 *   DESCRIPTION:	Swaps video pages
 *   INPUTS:		int terminal_num - Indicates which terminal
 *					int foreground	 - Indicates which action to take
 *   OUTPUTS: 		None
 *   RETURN VALUE:	None
 *   SIDE EFFECTS:	Swaps video pages
 */
void swap_video_pages(int terminal_num, int foreground){
	if(foreground){
		video_pg_table[0] = 0xB8000 | 7;
	}
	else{
		video_pg_table[0] = (0xB8000+(terminal_num * 0x1000)) | 7;
	}
	asm volatile("mov %0, %%cr3":: "b"(page_dir));
}	

/*
 *  update_screen_x_y
 *   DESCRIPTION:	Updates the current pcb's screen position variables
 *   INPUTS: 		pcb - pointer to pcb to update
 *   OUTPUTS: 		None
 *   RETURN VALUE: 	None
 *   SIDE EFFECTS: 	pcb values updated 
 */
void update_screen_x_y(pcb_t * pcb){
	pcb -> screen_x = get_screen_x(); 
	pcb -> screen_y = get_screen_y();
}

/*
 * update_parent_video
 *   DESCRIPTION:	Updates parent pcb's video memory variables
 *   INPUTS: 		pcb - pointer to pcb whose parent needs to be updated
 *   OUTPUTS: 		None
 *   RETURN VALUE: 	None
 *   SIDE EFFECTS:	parent's pcb video variables updated
 */
void update_parent_video(pcb_t * pcb){
	pcb_t * par = pcb -> parent_pcb;
	par -> screen_y = pcb -> screen_y;
	par -> screen_x = pcb -> screen_x;
}

/*
 * clear_foregrounds
 *	FUNCTION:		Clears out all the terminals except the one 
 *					corresponding to the argument passed in
 *	INPUT:			int skip - determines which terminal
 *					not to clear
 *	OUTPUT:			None
 *	RETURN VALUE:	None
 *	SIDE EFFECTS:	Clears a terminal, sets it to not active
 */
void clear_foregrounds(int skip){
	int pos;
	for(pos = 1; pos < 4; pos++){
		if(pos == skip){
			continue;
		}
		active_terminals[pos] = 0;
	}
}

/*
 * switch_terminal
 *   DESCRIPTION:	Jumps to terminal with the given terminal number 
 *					or starts a new terminal if no existing terminal.
 *   INPUTS: 		num - terminal number to switch to (1, 2, or 3)
 *   OUTPUTS: 		Overwrites video memory
 *   RETURN VALUE: 	None
 *   SIDE EFFECTS: 	Jumps to given terminal
 */
void switch_terminal(int num){
	cli();
	
	if(num < 1 || num > 3){
		printf("Only 3 terminals are allowed.\n");
		return;
	}
	
	//update current terminal variables
	cur_terminal = num;
	update_cur_terminal(num);
	active_terminals[cur_terminal] = 1;
	if(current_pcb != NULL){
		clear_foregrounds(num);
		copy_video_mem_out(current_pcb -> terminal_id);
		update_screen_x_y(current_pcb);
	}
	
	swap_video_pages(cur_terminal, 1);
	
	//first execution of new terminal
	if(open_terminals[num] == 0){
		clear();
		execute((uint8_t *)"shell");
	}
	
	//resume execution of existing terminal
	pcb_t * pcb = get_pcb(active_process[num]);
	jump_to_process(pcb -> pid);
}

/*
 * update_cur_pcb
 * FUNCTION: 		Updates local pointers
 * INTPUT: 			new_pcb - pointer to new pcb
 * OUTPUT: 			None
 * RETURN VALUE: 	Local pointers updated with new values
 * SIDE EFFECTS:	None
 */
void update_cur_pcb(pcb_t * new_pcb){
	current_pcb = new_pcb;
	file_array = new_pcb -> file_array;  //(file_t *)((uint8_t *)new_pcb + FILE_ARRAY_OFFSET);
	used_desc =  new_pcb -> used_desc;   //(char *)current_pcb;
	update_cur_buf(new_pcb);
}

/*
 * sys_call_pd_addrs
 *	FUNCTION: 		Updates local page directory pointer
 *	INTPUT:			page_directory - pointer to page directory
 *	OUTPUT: 		None
 *	RETURN VALUE: 	None
 *	SIDE EFFECTS:	Local pointer updated
 */
void sys_call_pd_addrs(unsigned int * page_directory){
	page_dir = page_directory;
}

/*
 * update_video_page_pointer
 *	FUNCTION:		Updates the local video page table pointer
 *	INPUT:			unsigned int * new_video_page - New address for the
 *													local video_pg_table
 *													pointer
 *	OUTPUT:			None
 *	RETURN VALUE:	None
 *	SIDE EFFECTS:	Updates local video_pg_table pointer
 */
void update_video_page_pointer(unsigned int * new_video_page){
	video_pg_table = new_video_page;
}

/*
 * get_pcb
 *	FUNCTION:		Returns pointer to pcb of process with pid 
 *	INTPUT:			pid - process id of process to find pcb for
 *	OUTPUT:			None
 *	RETURN VALUE: 	Pointer to pcb struct
 *	SIDE EFFECTS:	None
 */
pcb_t * get_pcb(int pid){
	return (pcb_t *)(0x800000 - (pid*8192));   //pcb located at (bottom 8KB * process id) of kernel 
}

/*
 * update_addrs
 *	FUNCTION: 		Grabs important memory address from stack immediately after interrupts
 *	INTPUT: 		None
 *	OUTPUT: 		pcb values updated with addresses to return to
 *	RETURN VALUE: 	None
 *	SIDE EFFECTS:	None
 */
void update_addrs(){
	asm volatile("movl %%esp, %0"
				: "=r"(t_esp)
	);
	t_esp += 32;
	
	pcb_t * pcb = get_pcb(active_process[cur_terminal]);
	pcb -> ret_eip = *(t_esp);
	pcb -> ret_cs = *(t_esp+1);
	pcb -> ret_flags = *(t_esp+2);
	pcb -> ret_ebp = *(t_esp-7);
	pcb -> ret_esp = (uint32_t)(t_esp+3);
}

/*
 * clear_pcb
 *	FUNCTION: 		Clears PCB entry with given PID
 *	INTPUT: 		int pid - process ID
 *	OUTPUT:			None
 *	RETURN VALUE:	None
 *	SIDE EFFECTS: 	pcb entry with given pid has pid set to 0, therefore
 *					marked for free use
 */
void clear_pcb(int pid){
	get_pcb(pid) -> pid = 0;
}

/*
 * clear_video_mem
 *	FUNCTION: 		Clears video memory related to given PID
 *	INTPUT: 		int pid - Process ID
 *	OUTPUT:			None
 *	RETURN VALUE:	None
 *	SIDE EFFECTS:	Clears video memory associated with pid
 */
void clear_video_mem(int pid){
	clear();
	copy_video_mem_out(pid);
}

/*
 * jump_to_process
 *	FUNCTION: 		Jumps to the process with the given PID
 *	INTPUT:			pid - Process id of process to jump to
 *	OUTPUT:			None 
 *	RETURN VALUE:	None
 *	SIDE EFFECTS:	Jumps to given process
 */
void jump_to_process(int pid){
	cli();
	
	uint32_t flags, eip, esp, ebp, ss, ds, cs, ret_val;
	
	asm volatile("movl %%eax, %0"
			:"=r" (ret_val)
	);
	
	//update virtual rtc
	change_to_virtual_rtc(pid);
	
	//update 128MB page to process's page
	page_dir[32] = (0x800000 + (0x400000 * (pid-1))) | 0x87;
	asm volatile("mov %0, %%cr3":: "b"(page_dir));
	
	pcb_t * pcb = get_pcb(pid);
	
	//update terminal with new process's information
	terminal_enter_off();
	
	//update pointers to video variables and current pcb
	update_cur_pcb(pcb);
	update_pointers(pcb, 1);
	
	esp = pcb -> ret_esp;
	ebp = pcb -> ret_ebp;
	eip = pcb -> ret_eip;
	cs = pcb -> ret_cs;
	flags = pcb -> ret_flags;
	
	copy_video_mem_in(pcb -> terminal_id);
	write_terminal_number(pcb -> terminal_id);
	
	if(active_terminals[cur_terminal] == 1){
		print_buffer();
	}
	
	//jump to kernel code
	if(cs == KERNEL_CS){
		ss = KERNEL_CS;
		ds = KERNEL_DS;
		
		//update tss
		tss.esp0 = (uint32_t)((uint8_t *)pcb + 8192);
		tss.ss0 = KERNEL_DS;

		asm volatile("movl %0, %%esp\n\t"
				"movl %3, %%ebp\n\t"
				"pushl %1\n\t"
				"popfl \n\t"
				"pushl %2\n\t"
				"movl %4, %%eax\n\t"
				"ret"
				: 
				: "r"(esp), "r"(flags), "r"(eip), "r"(ebp), "r"(ret_val)
		);
	}
	//jump to user code
	else if(cs == USER_CS){
		ss = USER_DS;
		ds = USER_DS;
		
		//update tss
		tss.esp0 = (uint32_t)((uint8_t *)pcb + 8192);
		tss.ss0 = KERNEL_DS;
		
		asm volatile("pushl %5\n\t"    //ss
					"pushl %1\n\t"     //esp
					"pushl %2\n\t"     //flags
					"pushl %3\n\t"     //cs
					"pushl %4\n\t"     //eip
					"movl %5, %%ds\n\t"
					"movl %5, %%es\n\t"
					"movl %0, %%ebp\n\t"
					: 
					: "r" (ebp), "r"(esp), "r"(flags) ,"r"(cs), "r"(eip), "r"(ds)
		);
		
		asm volatile("movl %0, %%eax\n\t"
					 "iret"
					:
					: "r" (ret_val)
		);
	}
	else{
		clear();
		swap_video_pages(1, 1);
		printf("bad pid: %d\n", pid);
		asm volatile(".17: hlt; jmp .17;");
	}//wrong cs, dont jump, pointers are wrong
	
}

/*
 * print_buffer
 *	FUNCTION:		Prints the terminal buffer to screen
 *	INPUT:			None
 *	OUTPUT:			Prints buffer
 *	RETURN VALUE:	None
 *	SIDE EFFECTS:	None
 */
void print_buffer(){
	int x = get_screen_x();
	int y = get_screen_y();
	int y_old = y;
	//puts(current_pcb -> terminal_buffer);
	int i;
	int j = 0;
	for(i = 0; i<1024; i++){
		j++;
		if(current_pcb -> terminal_buffer[i] == '\0'){
			break;
		}
		putc(current_pcb -> terminal_buffer[i]);
		if (current_pcb -> terminal_buffer[i] == '\0')
			j--;
		
		if ((j >= 72)&&(y_old == 24))
		{
			if (j == 72)
				y--;
			else if ((j-72)%80 == 0)
				y-=2;
		}
	}
	set_screen_x_y(x, y);
}

/*
 * store_state
 *	FUNCTION:		Stores the state of the process that is
 *					running in the currently visible terminal
 *	INPUT:			None
 *	OUTPUT:			None
 *	RETURN VALUE:	None
 *	SIDE EFFECTS:	Stores the current state of the process 
 *					in the current terminal
 */
void store_state(){
	uint32_t cur_cs;
	if(num_process == 1){
		return;
	}
	uint32_t * pit_esp;
	asm volatile("movl %%esp, %0\n\t"
				 "movl %%cs, %1"
				: "=r"(pit_esp), "=r" (cur_cs)
	);
	pit_esp += 24;
	
	pcb_t * pcb = get_pcb(active_process[cur_terminal]);
	pcb -> ret_eip = *(pit_esp);
	pcb -> ret_cs = *(pit_esp+1);
	pcb -> ret_flags = *(pit_esp+2);
	pcb -> ret_ebp = *(pit_esp-7);
	//switched from kernel mode
	if(cur_cs == pcb -> ret_cs){
		pcb -> ret_esp = (uint32_t)(pit_esp+3);
	}
	//switched from user mode
	else{
		pcb -> ret_esp = *(pit_esp+3);
	}
}

/*
 * increment_cur_process
 *	FUNCTION:		Increments current process. If we passed
 *					the maximum number of processes (3), we
 *					reset the value to 1
 *	INPUT:			None
 *	OUTPUT:			None
 *	RETURN VALUE:	None
 *	SIDE EFFECTS:	Modifies global variables
 */
void increment_cur_process(){
	cur_process++;
	if(cur_process == 4){
		cur_process = 1;
	}
}

/*
 * get_next_process
 *	FUNCTION:		Determines which process comes next
 *					for the scheduler
 *	INPUT:			None
 *	OUTPUT:			None
 *	RETURN VALUE:	Returns the next process
 *	SIDE EFFECTS:	None
 */
int get_next_process(){
	cli();
	
	if(num_process == 1){
		return 0;
	}
	
 	increment_cur_process();
	while(active_process[cur_process] == 0){
		increment_cur_process();
	}
	cur_terminal = cur_process;
	update_cur_terminal(cur_terminal);
	
	if(current_pcb != NULL){
		copy_video_mem_out(current_pcb -> terminal_id);
		update_screen_x_y(current_pcb);
	}
	
	swap_video_pages(cur_terminal, active_terminals[cur_terminal]);
	
	return active_process[cur_process];
}

/*
 * switch_to_active_terminal
 *	FUNCTION:		Updates the current PCB with the information
 *					relating to the currently active terminal
 *	INPUT:			None
 *	OUTPUT:			None
 *	RETURN VALUE:	None
 *	SIDE EFFECTS:	Updates the current PCB;
 *					Disables interrupts
 */
void switch_to_active_terminal(){
	cli();
	int i, new_term;
	for (i = 1; i < 4; i++){
		if (active_terminals[i] == 1){
			new_term = i;
		}
	}
	pcb_t * pcb = get_pcb(active_process[new_term]);
	if(new_term == cur_terminal){
	}
	else{
		update_cur_pcb(pcb);
		update_pointers(pcb, 0);
	}
}

/*
 * return_to_terminal
 *	FUNCTION:		Updates the current PCB with the information
 *					relating to the currently active terminal
 *	INPUT:			None
 *	OUTPUT:			None
 *	RETURN VALUE:	None
 *	SIDE EFFECTS:	Updates the current PCB
 */
void return_to_terminal(){
	cli();
	int i, new_term;
	for (i = 1; i < 4; i++){
		if (active_terminals[i] == 1){
			new_term = i;
		}
	}
	pcb_t * pcb = get_pcb(active_process[cur_terminal]);
	if(new_term == cur_terminal){
	}
	else{
		update_cur_pcb(pcb);
		update_pointers(pcb, 0);
	}
}

/*
 * schedule_active_terminal
 *	FUNCTION:		Changes terminal_waiting to the active terminal
 *	INPUT:			None
 *	OUTPUT:			None
 *	RETURN VALUE:	None
 *	SIDE EFFECTS:	Changes terminal_waiting to the active terminal
 */
void schedule_active_terminal()
{
	//Determine Active Terminal
	int i;
	for (i = 1; i < 4; i++)
	{
		if (active_terminals[i] == 1)
		{
			terminal_waiting = i;
			return;
		}
	}
}

#endif /* _SYS_CALLS_C */
