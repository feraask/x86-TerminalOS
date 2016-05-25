/*
	Header file for system calls.
*/

#ifndef _SYS_CALLS_H
#define _SYS_CALLS_H

#ifndef ASM

#include "x86_desc.h"
#include "types.h"
#include "filesystem.h"
#include "pit.h"
#include "i8259.h"

#define ARGS_MAX 32

volatile int terminal_waiting; 

int32_t halt(uint8_t status);
int32_t execute(const uint8_t* command);
int32_t read(int32_t fd, void* buf, int32_t nbytes);
int32_t write(int32_t fd, const void* buf, int32_t nbytes);
int32_t open(const uint8_t* filename);
int32_t close(int32_t fd);
int32_t getargs(uint8_t* buf, int32_t nbytes);
int32_t vidmap(uint8_t** screen_start);
int32_t set_handler(int32_t signum, void* handler_address);
int32_t sigreturn(void);
void switch_terminal(int num);
void update_addrs();
void update_screen_x_y(pcb_t * pcb);
void update_parent_video(pcb_t * pcb);
void copy_video_mem_out(int terminal_id);
void copy_video_mem_in(int terminal_id);
void swap_video_pages(int terminal_num, int foreground);
void update_video_page_pointer(unsigned int * new_video_page);
void sys_call_pd_addrs(unsigned int * page_directory);
pcb_t * get_pcb(int pid);
void update_cur_pcb(pcb_t * new_pcb);
void clear_pcb(int pid);
void clear_video_mem(int pid);
void jump_to_process(int pid);
void store_state();
int get_next_process();
void clear_foregrounds();
void print_buffer();

void switch_to_active_terminal();
void return_to_terminal();

void schedule_active_terminal();

#endif
#endif /* _SYS_CALLS_H */
