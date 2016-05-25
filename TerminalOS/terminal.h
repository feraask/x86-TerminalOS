/* terminal.h
 * Header for terminal driver
 */

#include "lib.h"
#include "filesystem.h"
#include "sys_calls.h"

void terminal_init();

int32_t terminal_open();
int32_t terminal_read(void* buf, int32_t nbytes);
int32_t terminal_write(const void* buf, int32_t nbytes);
int32_t terminal_close();

void terminal_backspace();
void terminal_enter();
void terminal_enter_off();
void terminal_clear();
void clear_pressed();

void set_CRTC_registers();
void update_cursor(int row, int col);

void update_cur_terminal(int new_cur_terminal);
void update_cur_buf(pcb_t * new_cur_pcb);
void print_buf();
void update_pointers(pcb_t * pcb, int option);
