/* keyboard.c - Keyboard Driver */

#include "keyboard.h"

/***********************KEYBOARD VARIABLES*********************************/
/*Shifts detected*/
int shift_ON;

/*Ctl detected*/
int ctl_ON;

//alt detected
int alt_ON;

/*CapsLock ON/OFF*/
int caps_lock;

/*scancode printable character lookup*/ 
char row_1[] = {'1','2','3','4','5','6','7','8','9','0','-','='};
char row_2[] = {'q','w','e','r','t','y','u','i','o','p','[',']'};
char row_3[] = {'a','s','d','f','g','h','j','k','l',';','\'','`','\\'};
char row_4[] = {'z','x','c','v','b','n','m',',','.','/',' '};

/*[SHIFT_ON]scancode printable character lookup*/
char shift_row_1[] = {'!','@','#','$','%','^','&','*','(',')','_','+'};
char shift_row_2[] = {'Q','W','E','R','T','Y','U','I','O','P','{','}'};
char shift_row_3[] = {'A','S','D','F','G','H','J','K','L',':','"','~','|'};
char shift_row_4[] = {'Z','X','C','V','B','N','M','<','>','?'};

/***************Private Keyboard Functions**********************************/

/*
 * print_scancode_to_terminal
 *	FUNCTION:		Tells terminal a character to write
 *	INPUT:			uint16_t scancode - Contains scancode 
 *					(value received from a keyboard interrupt)
 *	OUTPUT:			Prints a character to screen
 *	RETURN VALUE:	None
 *	SIDE EFFECTS:	None
 */
void print_scancode_to_terminal(uint16_t scancode)
{
	//Buffer to pass to terminal
	char buf[1];

	//Row 1 (scancodes from 0x02-0x0D)
	if (scancode <= 0x0D)
	{
		if (shift_ON)
			buf[0] = shift_row_1[scancode - 0x02];
		else
			buf[0] = row_1[scancode - 0x02];
	}
	//Row 2 (scancodes from 0x10-0x1B)
	else if (scancode <= 0x1B)
	{
		if (shift_ON || (caps_lock & (scancode <= 0x19)))
			buf[0] = shift_row_2[scancode - 0x10];
		else
		{
			buf[0] = row_2[scancode - 0x10];
		}
	}
	//Row 3 (scancodes from 0x1E-0x28) with 0x29(`~) and 0x2B(\|)
	else if (scancode <= 0x28)
	{
		if (shift_ON || (caps_lock & (scancode <= 0x26)))
			buf[0] = shift_row_3[scancode - 0x1E];
		else
			buf[0] = row_3[scancode - 0x1E];
	}
	else if (scancode == 0x29)
	{
		if (shift_ON)
			buf[0] = shift_row_3[11];
		else
			buf[0] = row_3[11];
	}
	else if (scancode == 0x2B)
	{
		if (shift_ON)
			buf[0] = shift_row_3[12];
		else
			buf[0] = row_3[12];
	}
	//Row 4 (scancodes from 0x2C-0x35) with 0x39(Space)
	else if (scancode == 0x39)
	{
		buf[0] = row_4[10];
	}
	else
	{
		if (shift_ON || (caps_lock & (scancode <= 0x32)))
			buf[0] = shift_row_4[scancode - 0x2C];
		else
			buf[0] = row_4[scancode - 0x2C];
	}
	
	//Tell terminal to write character here
	switch_to_active_terminal();
	terminal_write(buf, 1);
	return_to_terminal();
}
/***************************************************************************/

/*Initialize keyboard variables*/
/*
 * init_keyboard
 *	FUNCTION:		Initializes keyboard variables. These variables
 *					are used to determine if the Shift, Ctrl, and
 *					Caps Lock keys are on or off. Initially, these
 *					values are off (0).
 *	INPUT:			None
 *	OUTPUT:			None
 *	RETURN VALUE:	None
 *	SIDE EFFECT:	Initializes global variables for
 *					the keyboard.
 */
void init_keyboard()
{
	// Turn shift sensed to off
	shift_ON = 0;
	
	// Turn ctl sensed to off
	ctl_ON = 0;
	
	// Turn capslock off
	caps_lock = 0;
}

/* print_scancode
 *	FUNCTION:		Determines what should be done with a keyboard scancode.
 *	INPUT:			uint16_t scancode - Contains the scancode containing 
 *					what was pressed on the keyboard
 *	OUTPUT:			Action depends on the pressed key(s).
 *					Cases:
 *						Ctrl, Shift, Alt, Caps Lock:
 *							Modify corresponding global variables
 *						Enter:		
 *							Submits command
 *						Backspace:	
 *							Delete last element of terminal buffer
 *						Ctrl+L:
 *							Clears terminal screen
 *						Ctrl+C:
 *							Ends program (not implemented)
 *						Alt+#:
 *							Switches to terminal #
 *							# represents a number between 1 and 3
 *						Default:
 *							Prints character to screen (if valid)
 *	RETURN VALUE:	None
 *	SIDE EFFECT:	None
 */
void print_scancode(uint16_t scancode)
{	
	//printf("scancode: %x\n", scancode);
	switch(scancode)
	{
		//If shift pressed turn shift_ON
		case 0x2A: //LShift
		case 0x36: //RShift
			shift_ON++;
			break;
		
		//If shift released turn shift_ON off
		case 0xAA: //LShift
		case 0xB6: //RShift
			shift_ON--;
			break;
		
		//If alt pressed turn alt_ON
		case 0x38: //either alt
			alt_ON++;
			break;
		
		//If alt released turn alt_ON off
		case 0xB8: //either alt
			alt_ON--;
			break;
			
		//Toggle capsLock
		case 0x3A:
			if (caps_lock == 0)
				caps_lock = 1;
			else
				caps_lock = 0;
			break;
			
		//If ctl pressed turn ctl_ON
		case 0x1D:
			ctl_ON++;
			break;
		
		//If ctl released turn ctl_ON off
		case 0x9D:
			ctl_ON--;
			break;
		
		//Tell terminal enter is pressed
		case 0x1C:
			//Terminal Function for enter pressed here
			switch_to_active_terminal();
			terminal_enter();
			return_to_terminal();
			break;
			
		//Tell terminal backspace pressed
		case 0x0E:
			//Terminal Function for backspace pressed here
			switch_to_active_terminal();
			terminal_backspace();
			return_to_terminal();
			break;
			
		//Check if ctl-l is pressed
		case 0x26:
			if (ctl_ON)
			{
				//Terminal Function for clear here
				switch_to_active_terminal();
				clear_pressed();
				return_to_terminal();
			}
			else
			{
				print_scancode_to_terminal(scancode);
			}
			break;
			
		//Check if ctl-c is pressed
		case 0x2E:
			if (ctl_ON)
			{
				//Terminal Function for halt process here
			}
			else
				print_scancode_to_terminal(scancode);
			break;
			
		//Check for alt + Fn 1
		case 0x3B:
			if (alt_ON){
				send_eoi(1);
				update_addrs();
				switch_terminal(1);
			}
			break;
			
		//Check for alt + Fn 2
		case 0x3C:
			if (alt_ON){
				send_eoi(1);
				update_addrs();
				switch_terminal(2);
			}
			break;
			
		//Check for alt + Fn 3
		case 0x3D:
			if (alt_ON){
				send_eoi(1);
				update_addrs();
				switch_terminal(3);
			}
			break;
			
		default:
			//Check if valid character to print
			if ((scancode <= 0x35) || (scancode == 0x39))
				print_scancode_to_terminal(scancode);
			break;
	}

}
