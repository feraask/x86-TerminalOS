#include "rtc.h"

volatile float running_freq = 0;
volatile int current_rtc = 0;					//Index of current virtual RTC
volatile virtual_rtc_t virtual_rtc[6];			//Array of virtual RTCs
volatile int active_rtc[6] = {0,0,0,0,0,0};		//Array of active virtual RTC's

/***************************Private RTC Function(s)*************************************/

/*
 * change_RTC_freq
 * DESCRIPTION:	Changes the frequency at which the RTC generates exceptions.
 *				The RTC takes in a value to shift the frequency (it does not
 *				take in the frequency directly). This value is calculated in
 *				the function based off the given frequency.
 * INPUT:	uint32_t freq - Corresponds to the desired frequency in Hertz
 * OUTPUT:	Modifies the frequency of interrupts of the RTC.
 * RETURN VALUE:	None
 * SIDE EFFECTS:	None
 */
void change_RTC_freq(uint32_t freq)
{
	if (freq <= running_freq && freq != 0)
	{
		return;
	}
    uint8_t rate;
	
	/* 
	 * Frequency = 32768 >> (rate - 1) and rate = log_2(32768/freq)+1
	 * log_2 == log base 2
	 * The switch statement was used because, while less elegant, it's a more efficient solution than
	 * dealing having to evuluate a logarithmn, along a division and an addition. It's an acceptable
	 * alternative design choice since we only accept a fairly limited range of frequencies (2 to 1024 Hz).
	 * The RTC accepts a rate value between 2 and 15, however since we've limited our maximum frequency to
	 * 1024 Hz, the rate now ranges from 6 to 15.
	 */
	switch(freq)
	{
		case 2:
			rate = 15;
			break;
		case 4:
			rate = 14;
			break;
		case 8:
			rate = 13;
			break;
		case 16:
			rate = 12;
			break;
		case 32:
			rate = 11;
			break;
		case 64:
			rate = 10;
			break;
		case 128:
			rate = 9;
			break;
		case 256:
			rate = 8;
			break;
		case 512:
			rate = 7;
			break;
		case 1024:
			rate = 6;
			break;
	/*If frequency is 0, then this is being called from the rtc_close*/
		default:
			//Only close if it is the last open virtual rtc
			active_rtc[current_rtc] = 0;
			int i;
			for(i=0; i<6; i++)
			{
				//Check if another virutal rtc is open
				if(active_rtc[i] == 1)
					return;
			}
			rate = 0;
			running_freq = 0;
	}
	/*Make sure rate <= 15*/
	rate &= 0x0F;
	running_freq = freq;

	cli();
	outb(0x8A, 0x70);			// select register A, and disable NMI
	char prev = inb(0x71);		// read the current value of register A
	outb(0x8A, 0x70);			// select register A, disable NMI
	outb((prev & 0xF0) | rate, 0x71);		//Set the rate, re-enable NMI
	sti();
}



/***************************Public RTC Functions*************************************/
/*
 * tick
 *   DESCRIPTION: Called by interrupt handler to signify that an interrupt has occured.
 *   INPUTS: None
 *   OUTPUTS: None
 *   RETURN VALUE: None
 *   SIDE EFFECTS: Modifies virtual RTC counter
 */
void tick()
{
	cli();
	int i;
	for(i=0; i<6; i++)
	{
		if(active_rtc[i] == 1)
			virtual_rtc[i].counter += (virtual_rtc[i].freq/running_freq);
	}
	sti();
}

/*
 * change_to_virtual_rtc
 *   DESCRIPTION: 	Switch the current RTC being used based of the current process
					being run.
 *   INPUTS:	  	int pid - process ID of current process
 *   OUTPUTS:	  	None
 *   RETURN VALUE:	None
 *   SIDE EFFECTS:	None
 */
void change_to_virtual_rtc(int pid)
{
	current_rtc = pid - 1;
}

/*
 * rtc_init
 *   DESCRIPTION: 	Initialize virtual rtc array
 *   INPUTS:		None
 *   OUTPUTS:		None
 *   RETURN VALUE:	None
 *   SIDE EFFECTS:	Initialize virtual_rtc values
 */
void rtc_init()
{
	//Set up virtual rtcs
	virtual_rtc_t v_rtc;
	v_rtc.counter = 0;
	v_rtc.freq = 0;
	
	int i;
	for(i=0; i<6; i++)
	{
		virtual_rtc[i] = v_rtc;
	}
}

/*
 * rtc_open
 *   DESCRIPTION: 	Turns on the rtc and sets the frequency to 2Hz.
 *					Frequency is set using the change_RTC_freq function.
 *					Frequency is set to 2 Hz as specified.
 *   INPUTS: None
 *   OUTPUTS: Sets the RTC frequency to 2 Hz (minimum accepted frequency)
 *   RETURN VALUE: 0
 *   SIDE EFFECTS: Enables RTC interrupts to occur.
 */
int32_t rtc_open()
{
    cli();
	
	int i;
	for(i=0; i<6; i++)
	{
		//Check if rtc already open
		if(active_rtc[i] == 1)
		{
			active_rtc[current_rtc] = 1;
			return 0;
		}
	}
	
	rtc_init();

    /*Turn on the RTC initially to default rate of 1024Hz*/
	outb(0x8B, 0x70);			// select register B, and disable NMI
	char prev = inb(0x71);		// read the current value of register B
	outb(0x8B, 0x70);			// set the index again (a read will reset the index to register D)
	outb(prev | 0x40, 0x71);	// write the previous value ORed with 0x40. This turns on bit 6 of register B
    sti();

    /*Set the frequency to 2Hz according to the spec*/
    change_RTC_freq(2);
	
	active_rtc[current_rtc] = 1;
	
	sti();
    return 0;
}

/*
 * rtc_read
 *   DESCRIPTION: 	Waits for an RTC interrupt to occur and then returns.
 *					The RTC interrupt handler modifies the value of 
 *					rtc_interrupt_occured and sets it to 1. When this 
 *					happens, rtc_interrupt_occured is set to 0 again and
 *					returns. 
 *   INPUTS: 		Takes in a pointer to a buffer and the number of bytes in the
 *					buffer, but neither input is used in the function. 
 *   OUTPUTS: None
 *   RETURN VALUE: 0
 *   SIDE EFFECTS: None
 */
int32_t rtc_read(void * buf, int32_t nbytes)
{	
	if (virtual_rtc[current_rtc].freq == 0)
		return 0;

	// We need to enable interrupts in order to
	// read an RTC interrupt
	sti();
	
	/*	
	 *	We loop until rtc_interrupt_occured is modified.
	 *	Since its only modified when an RTC interrupt
	 *	occurs, we are effectively waiting for an RTC
	 *	interrupt to occur.
	 */	
    while(virtual_rtc[current_rtc].counter < 1)
    {
    }
    cli();

	//Interrut has occured so reset counter
	virtual_rtc[current_rtc].counter = 0;
    sti();
	
	return 0;
}

/*
 * rtc_write
 *   DESCRIPTION: 	Changes RTC frequency to whatever is passed in the buffer
 *   INPUTS:		const void * buf - 	pointer to a buffer (which contains the 
 *										desired frequency in Hz), 
 *					int32_t nbytes   - 	Max number of bytes to put in the buffer
 *   OUTPUTS: 		None
 *   RETURN VALUE: 	Return number of bytes written (4) on success, and 
 *					return -1 on failure.
 *   SIDE EFFECTS: 	None
 */
int32_t rtc_write(const void* buf, int32_t nbytes)
{
	//sti();
	cli();

    //Check if buffer is a valid pointer
    if(buf == 0x00)
        return -1;

	uint32_t * buffer = (uint32_t *) buf;
    uint32_t freq = buffer[0];
    //Check if the frequency passed in is an acceptable value for the RTC
    if(freq > 1024 || freq < 1)
        return -1;
    if(freq != 1 && freq%2 != 0)
        return -1;

	/*Set the frequency based on what was passed in*/
    change_RTC_freq(freq);
	virtual_rtc[current_rtc].freq = freq;
	
	//sti();

	return 4;
}

/*
 * rtc_close
 *   DESCRIPTION: 	Turns off the RTC interrupts by setting
 *					the RTC frequency to 0.
 *   INPUTS: 		int32_t fd - File descriptor; not used in the function
 *   RETURN VALUE: 	0
 *   SIDE EFFECTS: 	Modifies the frequency of RTC interrupts
 *					(effectively disables it).
 */
int32_t rtc_close(int32_t fd)
{
    /*Settting frequency to 0 turns off RTC*/
    change_RTC_freq(0);
	return 0;
}
