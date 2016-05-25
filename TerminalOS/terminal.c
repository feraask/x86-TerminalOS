/* terminal.c
 * Implementation of terminal driver
 */
 
#include "terminal.h"

#define NUM_CRTC_REGS          25
#define NUM_COLS               80

/***********************Global Variables**************************/

/*Variables stored in process's PCB*/
int8_t * terminal_buffer;    //Terminal's read buffer of size [1024]
int * t_pos;                 //Position in the terminal read buffer
int * isReading;             //1 if the terminal is reading, 0 otherwise
int * enter_pressed;
int * clear_was_pressed;

int cur_terminal;            //The current terminal's number
pcb_t * cur_pcb;
char dbufarray[3][10];
int dposarray[3] = {0,0,0};


/*****************************************************************/

static unsigned short text_CRTC[NUM_CRTC_REGS] = {
    0x5F00, 0x4F01, 0x5002, 0x8203, 0x5504, 0x8105, 0xBF06, 0x1F07,
    0x0008, 0x4F09, 0x0D0A, 0x0E0B, 0x000C, 0x000D, 0x000E, 0x000F,
    0x9C10, 0x8E11, 0x8F12, 0x2813, 0x1F14, 0x9615, 0xB916, 0xA317,
    0xFF18
};

/*
 * set_CRTC_registers
 *   DESCRIPTION:	Set VGA cathode ray tube controller (CRTC) registers.
 *   INPUTS: 		table -- table of CRTC register values to use
 *   OUTPUTS: 		none
 *   RETURN VALUE: 	none
 *   SIDE EFFECTS: 	none
 */ 
void set_CRTC_registers(){
    /* clear protection bit to enable write access to first few registers */
    outw(0x03D4, 0x0011); 
    REP_OUTSW (0x03D4, text_CRTC, NUM_CRTC_REGS);
}

/*
 * update_cursor
 *	DESCRIPTION:	Updates the position of the terminal cursor
 *	INPUTS:			int row - y position of the new cursor location
 *					int col - x position of the new cursor location
 *	OUTPUT:			None
 *	RETURN VALUE:	None
 *	SIDE EFFECTS:	Updates cursor
 */
void update_cursor(int row, int col)
{ 
	outb(0x3D4, 14);     // write to register 14 first
	outb(0x3D5, 0); 	 // output high byte
	outb(0x3D4, 15);     // again to register 15
	outb(0x3D5, 8);      // low byte in this register
}
 
/*
 * terminal_init
 *   DESCRIPTION: Initialize variables for terminal
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: Terminal variables set to initial values
 */
void terminal_init(){
	//Nothing is being read or pressed
	*enter_pressed = 0;
	*isReading = 0;
	*clear_was_pressed = 0;
	
	//Terminal is the first terminal, this is changed if needed
	cur_terminal = 1;
	
	int i, j;
	for(j = 0; j<3; j++){
		for(i=0; i<10; i++){
			dbufarray[j][i] = '\0';
		}
	}
}

/*
 * terminal_open
 *   DESCRIPTION: 	Opens a new terminal
 *   INPUTS: 		None
 *   OUTPUTS: 		None
 *   RETURN VALUE: 	>=0 on success, <0 on failure
 *   SIDE EFFECTS: 	None
 */
int32_t terminal_open(){
	//will be modified to support opening multiple terminals later
	return -1;
}

/*
 * terminal_read
 *   DESCRIPTION:	Reads nbytes from the terminal and enters them into buf.  Reads up to nbytes or new line or null character.
 *   INPUTS: 		buf - buffer to fill
 *					nbytes - number of bytes to read
 *   OUTPUTS: 		none
 *   RETURN VALUE:	number of bytes actually read, -1 on error
 *   SIDE EFFECTS:	Modifies buf
 */
int32_t terminal_read(void* buf, int32_t nbytes){
	
	sti();
	
	//Check for valid buf
	if(buf == 0x0)
	{
		return -1;
	}
	
	//Clear the read buffer
	*t_pos = 0;
	int i;
	for (i = 0; i < 1024; i++)
	{
		terminal_buffer[i] = '\0';
	}
	
	//Read until enter is pressed
	*enter_pressed = 0;
	*isReading = 1;
	while(!(*enter_pressed))
	{
		//Wait for enter to be pressed
		terminal_clear();
	}
	*isReading = 0;
	
	//Fill buf with the terminal's read buffer
	int j = 0;
	for(i=0; i<nbytes; i++){
		j++;
		if(terminal_buffer[i] == '\n'){
			//found new line
			i++;
			break;
		}
		if(terminal_buffer[i] == '\0'){
			j--;
		}
		((char *)buf)[i] = terminal_buffer[i];
		
		if (j >= 72)
		{
			if (j == 72)
				increment_row();
			else if ((j-72)%80 == 0)
				increment_row();
		}
	}
	((char *)buf)[i] = '\0';     //terminating string character
	
	*t_pos = 0;
	for (i = 0; i < 1024; i++)
	{
		terminal_buffer[i] = '\0';
	}
	
	increment_row();
	
	//Return the number of bytes put into buf
	return i+1;
}

/*
 * terminal_write
 *   DESCRIPTION:	Writes nbytes from buf to the terminal.
 *   INPUTS: 		buf - buffer to read from
 *					nbytes - number of bytes to write
 *   OUTPUTS:		none
 *   RETURN VALUE:	number of bytes actually written
 *   SIDE EFFECTS:	Bytes written to screen
 */
int32_t terminal_write(const void* buf, int32_t nbytes){
	cli();
	terminal_clear();
	int i;
	for(i=0; i<nbytes; i++){
		int8_t cur_char = ((char *)buf)[i];
		
		//end of string to write
		if(cur_char == '\0'){
			return i;
		}
		
		//prevent read overflow
		if (*isReading == 1)
		{
			if (*t_pos >= 1024)
				break;
			
			terminal_buffer[*t_pos] = cur_char;
			dbufarray[cur_terminal-1][dposarray[cur_terminal-1]] = cur_char;
			dposarray[cur_terminal-1] += 1; 
			*t_pos= *t_pos+1;
		}
		else{
			//write character to screen
			if(cur_terminal == cur_pcb -> terminal_id){
				putc(cur_char);
			}
		}
	}

	sti();
	
	sti();
	
	return i;
}

/*
 * print_buf
 *	FUNCTION:		Prints the terminal buffer to screen
 *	INPUT:			None
 *	OUTPUT:			Prints to screen
 *	RETURN VALUE:	None
 *	SIDE EFFECTS:	None
 */
void print_buf(){
	int screen_x = get_screen_x();
	int screen_y = get_screen_y();
	int i;
	int j = 0;
	for(i = 0; i<1024; i++){
		j++;
		if(terminal_buffer[i] == '\0'){
			break;
		}
		putc(terminal_buffer[i]);
		if (terminal_buffer[i] == '\0')
			j--;
		terminal_buffer[i] = '\0';
		
		if ((j >= 72)&&(screen_y == 24))
		{
			if (j == 72)
				screen_y -= 2;
			else if ((j-72)%80 == 0)
				screen_y--;
		}
	}
	set_screen_x_y(screen_x,screen_y);
}

/*
 * terminal_close
 *   DESCRIPTION: Closes a terminal
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: >=0 on success, <0 on failure
 *   SIDE EFFECTS: none
 */
int32_t terminal_close(){
	return -1;
}

/*
 * terminal_backspace
 *   DESCRIPTION: Deletes character immediately before cursor
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: Characters printed to screen
 */
void terminal_backspace(){
	//Only backspace if the terminal is reading and at least 1 character has been typed
	if(*t_pos > 0 && *isReading == 1){
		*t_pos = *t_pos-1;
		terminal_buffer[*t_pos] = ' ';
					
		int screen_x = get_screen_x();
		if(screen_x % NUM_COLS == 0){
			decrement_row();    //go back a line
		}
		
		print_buffer();
	}
}
 
/*
 * terminal_enter
 *   DESCRIPTION: Executes command and clears buffer
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: Command executed
 */
void terminal_enter(){
	
	//Return from terminal read
	*enter_pressed = 1;
}

/*
 * terminal_enter_off
 *   DESCRIPTION: Turns off the global variable enter_pressed
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: enter_pressed = 0
 */
void terminal_enter_off(){
	*enter_pressed = 0;
}

/*
 * terminal_clear
 *   DESCRIPTION: Clears buffer and clears video memory
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: Buffer and video memory cleared
 */
void terminal_clear(){
	if (*clear_was_pressed == 1){
		//clear screen and read buffer
		clear();
		*t_pos = 0;
		int i;
		for (i = 0; i < 1024; i++)
		{
			terminal_buffer[i] = '\0';
		}
		
		//resets the terminal screen
		puts("391OS> ");
		write_terminal_number(cur_terminal);
		*clear_was_pressed = 0;
	}
}

/*
 * clear_pressed
 *   DESCRIPTION: Sets variable to signify a clear command
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: clear_was_pressed is set to 1
 */
void clear_pressed(){
	*clear_was_pressed = 1;
}

/*
 * update_cur_terminal
 *   DESCRIPTION: Changes the number of the terminal (1-3)
 *   INPUTS: new_cur_terminal - terminal's number
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: cur_terminal is set
 */
void update_cur_terminal(int new_cur_terminal){
	cur_terminal = new_cur_terminal;
}

/*
 * update_cur_buf
 *   DESCRIPTION: 	Swaps pcb with a new one to update the buffer
 *   INPUTS: 		pcb_t * new_cur_pcb - new pcb to replace current
 *   OUTPUTS: 		None
 *   RETURN VALUE: 	None
 *   SIDE EFFECTS: 	None
 */
void update_cur_buf(pcb_t * new_cur_pcb){
	cur_pcb = new_cur_pcb;
}

/*
 * update_pointers
 *   DESCRIPTION: Updates the current terminal's PCB pointers
 *   INPUTS: pcb - the current process's PCB
			 option - whether to update the screen position
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: cur_terminal's pointers are updated
 */
void update_pointers(pcb_t * pcb, int option){
	//update PCB variables
	terminal_buffer = pcb -> terminal_buffer;
	isReading = &(pcb -> isReading);
	enter_pressed = &(pcb -> enter_pressed);
	t_pos = &(pcb -> terminal_pos);
	clear_was_pressed = &(pcb -> clear_was_pressed);

	//update screen position
	if(option == 1){
		set_screen_x_y(pcb -> screen_x, pcb -> screen_y);
	}
}

