#include "pit.h"

/*
 * start_pit
 *   DESCRIPTION:	Starts the PIT
 *   INPUTS:		None
 *   OUTPUTS:		None
 *   RETURN VALUE:	None
 *   SIDE EFFECTS:	Enables the PIT
 */
void start_pit(){
	cli();
	
	//write init command to register 0x43
	//bits 6 - 7: 00  - channel 0
	//bits 4 - 5: 11  - access low byte / high byte
	//bits 1 - 3: 000 - interrupt on count = 0
	outb(0x30, 0x43);
	
	//write low byte to channel 0 (0x40)
	outb(0x00, 0x40);
	
	//write high byte to channel 0 (0x40)
	outb(0x00, 0x40);
	
	sti();
}

/*
 * change_PIT_freq (NOT IMPLEMENTED)
 *   DESCRIPTION:	Changes the frequency of the PIT
 *   INPUTS:		uint32_t freq - Desired frequency
 *   OUTPUTS:		None
 *   RETURN VALUE:	None
 *   SIDE EFFECTS:	Changes PIT frequency 
 */
void change_PIT_freq(uint32_t freq){

}
