/* filesystem.h
 * Header for file system driver
 */

#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H

#include "types.h"
#include "lib.h"

#define FILE_ARRAY_OFFSET (24+32)
#define EXE_OFFSET 0x48000

//Directory Entry Stucture
typedef struct dentry{
	//Name of File
	char file_name[32];

	//File Types
	// 0 - User Access to RTC
	// 1 - Directory
	// 2 - Regular File
	uint32_t file_type;

	// Index Node Number
	uint32_t inode_num; 
}dentry_t;
 
//file operations table structure
typedef struct file_operations{
	int32_t (*open)();
	int32_t (*read)(void* buf, int32_t nbytes);
	int32_t (*write)(const void* buf, int32_t nbytes);
}file_operations_t;

//file structure
typedef struct file {
	uint32_t f_pos;
	file_operations_t f_ops;
	uint32_t eof;
	dentry_t f_dentry;
}file_t;

//pcb structure 
typedef struct pcb{
	int pid;
	int terminal_id;
	int parent_pid;
	int read_pos;				   //terminal read position
	int isReading;				   //terminal is/isn't reading
	int8_t terminal_buffer[1024];  //terminal read buffer
	int terminal_pos;              //terminal read position
	int screen_x;                  //terminal variable from lib.c
	int screen_y;                  //terminal variable from lib.c
	int enter_pressed;
	int clear_was_pressed;
	struct pcb * parent_pcb;
	file_t file_array[8];
	char used_desc[8];
	uint32_t k_esp;
	uint32_t k_ebp;
	uint32_t ret_eip;
	uint32_t ret_esp;
	uint32_t ret_ebp;
	uint32_t ret_cs;
	uint32_t ret_flags;
	uint32_t parent_esp;
	int8_t args[32];
}pcb_t;


int32_t read_dentry_by_name (const uint8_t* fname, dentry_t* dentry);
int32_t read_dentry_by_index (uint32_t index, dentry_t* dentry);
int32_t read_dentry_by_dir_index (uint32_t index, dentry_t* dentry);
void print_dentry(dentry_t entry);
int32_t is_valid_cmd(dentry_t * executable, const uint8_t* program_name);
 
int32_t num_dir_entries();
void fs_init(uint32_t fs_start);
int32_t fs_open();
int32_t fs_write(const void* buf, int32_t nbytes);
int32_t fs_close();

 /* Buffer passed into fs_read defined                      *
  *															*
  * element 0 - uint32_t 									*
  *							0 if to read by name			*
  *							1 if to read by index			*
  * element 1 - 											*
  *			(element 0 = 0) start of char array of filename	*
  *			(element 0 = 1) uint32_t index to read  		*
  * element 2 -												*
  *							uint32_t offset					*
  *															*/
int32_t fs_read(void* buf, int32_t nbytes);

//Program loader
pcb_t * load_program(const uint8_t* program_name, uint32_t *esp, uint32_t *eip, int pid);

#endif
