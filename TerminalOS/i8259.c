/* i8259.c - Functions to interact with the 8259 interrupt controller
 * vim:ts=4 noexpandtab
 */

#include "i8259.h"
#include "lib.h"

#define MASTER_COMMAND		MASTER_8259_PORT		/* IO base address for master PIC */
#define SLAVE_COMMAND		SLAVE_8259_PORT			/* IO base address for slave PIC */
#define MASTER_DATA	(MASTER_COMMAND+1)				/*Data ports for writing to PICs*/
#define SLAVE_DATA	(SLAVE_COMMAND+1)

/* Interrupt masks to determine which interrupts
 * are enabled and disabled */
uint8_t master_mask; /* IRQs 0-7 */
uint8_t slave_mask; /* IRQs 8-15 */

/*
 * i8259_init
 *	FUNCTION:		Initializes the 8259 PIC (both master and slave PIC are initialized).
 *					Each ICW# is a step in the initialization, corresponding to the following:
 *						1. Begin the initialization sequence. (ICW1)
 *						2. Its vector offset. (ICW2)
 *						3. Tell it how it is wired to master/slaves. (ICW3)
 *						4. Gives additional information about the environment, like the mode to be used. (ICW4)
 *	INPUT:			None
 *	OUTPUT:			None
 *	RETURN VALUE:	None
 *	SIDE EFFECT:	PIC is initialized.
 */
void
i8259_init(void)
{
	/* starts the initialization sequence (in cascade mode)*/
	outb(ICW1, MASTER_COMMAND);
	outb(ICW1, SLAVE_COMMAND);

	/* ICW2: Master/Slave PIC vector offset (so when we read the IDT table we add
	this to the base address to find our vector)*/
	outb(ICW2_MASTER, MASTER_DATA);
	outb(ICW2_SLAVE, SLAVE_DATA);

	/* ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	and the slave PIC its identity is 2 (0000 0010)*/
	outb(ICW3_MASTER, MASTER_DATA);
	outb(ICW3_SLAVE, SLAVE_DATA);

	/* Tell the PIC's we are running in 8086/88 (MCS-80/85) mode */
	outb(ICW4, MASTER_DATA);
	outb(ICW4, SLAVE_DATA);

	/* Clear all masks. IRQ is disabled when bit is 1 */
	master_mask = slave_mask = 0xFF;
	outb(master_mask, MASTER_DATA);
	outb(slave_mask, SLAVE_DATA);
}

/*
 * disable_irq
 *   DESCRIPTION: 	Disable (unmask) the specified IRQ
 *   INPUTS:		uint32_t irq_num - Tells us which IRQ to disable
 *   OUTPUTS:		None
 *   RETURN VALUE:	None
 *   SIDE EFFECTS:	Unmasks the specified IRQ
 */
void
disable_irq(uint32_t irq_num)
{
	uint16_t port;
    uint8_t data;

	/* Determine whether we're using the master or slave PIC based on the irq_num
	 * and adjust it, if necessary. Then select the ports and set up the data. */
    if(irq_num < 8) {
        port = MASTER_8259_PORT+1;
		master_mask |= (1<<irq_num); /* 1<<irq_num contains a 1 in the bit-position of the irq_num and 0 elsewhere.
									  * For example, if irq_num = 3, (1<<irq_num) will be 0000 0100
									  * We want to adjust master_mask to include the irq_num we are unmasking. */
		data = master_mask;
	} else {
		port = SLAVE_8259_PORT+1;
		irq_num -= 8; /* Adjusts the irq_num. It's originally 8 larger than it should be to tell the difference between master and slave */
		slave_mask |= (1<<irq_num);
		data = slave_mask;
	}


	/* Send the data to the PIC	*/
	outb(data, port);
}

/*
 * enable_irq
 *   DESCRIPTION: 	Enables (mask) the specified IRQ
 *   INPUTS:		uint32_t irq_num - Tells us which IRQ to enable
 *   OUTPUTS:		None
 *   RETURN VALUE:	None
 *   SIDE EFFECTS:	The specified IRQ is enabled.
 */
void
enable_irq(uint32_t irq_num)
{
	uint16_t port;
    uint8_t data;

	/* Determine whether we're using the master or slave PIC based on the irq_num
	 * and adjust it, if necessary. Then select the ports and set up the data. */
    if(irq_num < 8) {
        port = MASTER_8259_PORT+1;
		master_mask &= ~(1<<irq_num); /* (1<<irq_num) contains a 1 in the bit position irq_num and 0 elsewhere.
									   * By using ~, it now contains a 0 at bit position irq_num and a 1 elsewhere.
									   * By using &, we set the bit of position irq_num in the mask to 0 */
		data = master_mask;
	} else {
		port = SLAVE_8259_PORT+1;
		irq_num -= 8; /* Adjusts the irq_num. It's originally 8 larger than it should be to tell the difference between master and slave */
		slave_mask &= ~(1<<irq_num);
		data = slave_mask;
	}

	/* Send the data to the PIC	*/
	outb(data, port);
}

/*
 * send_eoi
 *   DESCRIPTION: 	Sends the End of Interrupt (EOI) signal to the IRQ specified
 *					by the input. If the EOI is sent to an IRQ corresponding to the
 *					slave, an EOI must be sent to both the Slave and Master PICs.
 *   INPUTS:		uint32_t irq_num - Tells us which IRQ we're sending the EOI to
 *   OUTPUTS:		None
 *   RETURN VALUE:	None
 *   SIDE EFFECTS:	Sends an EOI signal.
 */
void
send_eoi(uint32_t irq_num)
{
	//Check if IRQ is on slave PIC, if so send EOI to both Master and Slave
	if(irq_num >= 8)
	{
		outb((EOI | (irq_num - 8)), SLAVE_COMMAND);
		outb((EOI | 0x02), MASTER_COMMAND);
		return;
	}

    //Only send EOI to Master (IRQ < 8)
    outb((EOI | irq_num), MASTER_COMMAND);

    return;

}

