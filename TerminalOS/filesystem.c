/* filesystem.c
 * Implementation of file system driver
 */
 
#include "filesystem.h"

#define _128MB	0x8000000
#define _8MB		0x800000
#define _4MB		0x400000
#define _8KB		8192

/***************************FILE SYSTEM GLOBAL VARIABLES*****************************/

//Address of the start of the file system
uint32_t fs = 0x0;

/***************************PRIVATE FILE SYSTEM FUNCTIONS****************************/

/*
 * num_dir_entries
 *   DESCRIPTION:	Returns the number of directory entries in the file system
 *   INPUTS:		None
 *   OUTPUTS:		None
 *   RETURN VALUE:	Returns the number of directory entries as an integer
 *   SIDE EFFECTS:	None
 */
int32_t num_dir_entries(){
	return (int32_t)*((uint32_t *)fs);
}

/*
 * read_dentry_by_name
 *   DESCRIPTION:	Reads a dentry by name. Essentially goes through
 *					the directory comparing the file name (fname) that
 *					was passed in with the name of the files in the directory.
 *					If a match is found, the dentry that was in is filled with
 *					the values of the dentry with the name that we were looking
 *					for and returns 0. If a match is not found, it returns -1.
 *   INPUTS:		const uint8_t * fname  - name of file to find
 *					dentry_t * 		dentry - empty dentry_t struct to fill
 *   OUTPUTS:		None
 *   RETURN VALUE:	0 on success and -1 on failure, in which success implies we
 *					found the dentry we were looking for
 *   SIDE EFFECTS:	The dentry is updated if a dentry with fname is found
 */
int32_t read_dentry_by_name (const uint8_t* fname, dentry_t* dentry){
	//Check for valid input
	if ((fname == NULL) || (dentry == NULL))
		return -1;

	//Return value (initially not found)
	int32_t retval = -1;

	// Get number of directory entries (First 4B of Boot Block)
	uint32_t num_dentries = *((uint32_t *)fs);
	
	// Get a pointer to the start of the directory entries (starting at 64B in the Boot Block)
	uint32_t dentries = fs + 64;

	// Loop through directory entries and check if fname matches the directory name
	int i;
	for (i = 0; i < num_dentries; i++)
	{
		//Check if directory entry file name matches fname
		//   dentries points to the filename of that directory entry
		if (strncmp((int8_t *)fname, (int8_t *)dentries, 32) == 0)
		{
			// Directory Found! Update dentry and return success (0)
			//    Update dentry file name
			int j;
			for(j = 0; j <32; j++)
			{
				dentry->file_name[j] = *((uint32_t *)(dentries + j));
			}
			
			//    Update dentry file type
			dentry->file_type = *((uint32_t *)(dentries + 32));
			
			//    Update dentry index node number
			dentry->inode_num = *((uint32_t *)(dentries + 36));
			
			retval = 0;
			break;
		}
		else
		{
			// Not this directory continue looking (64B later is the next dentry)
			dentries += 64;
		}
	}
	
	return retval;
}

/*
 * read_dentry_by_index
 *   DESCRIPTION:	Fills given dentry_t struct with info from inode at given index
 *   INPUTS: 		index  - integer index of inode in file system
 *					dentry - empty dentry_t struct to fill
 *   OUTPUTS: 		None
 *   RETURN VALUE: 	0 on success, -1 on failure 
 *   SIDE EFFECTS: 	None
 */
int32_t read_dentry_by_index (uint32_t index, dentry_t* dentry){

	//check for null dentry
	if(dentry == 0x0){
		return -1;
	}
	
	// Get number of directory entries (First 4B of Boot Block)
	uint32_t num_dentries = *((uint32_t *)fs);
	
	// Get a pointer to the start of the directory entries (starting at 64B in the Boot Block)
	uint32_t dentries = fs + 64;

	// Loop through directory entries and check if fname matches the directory name
	int i;
	for (i = 0; i < num_dentries; i++){
		if(*((int32_t *)(dentries+36)) == index){  //found dentry match based on inode num
			int j;
			//copy file name character by character
			for(j=0; j<32; j++){
				dentry->file_name[j] = *((uint32_t*)(dentries+j));
			}
			dentry->file_type = *((int32_t*)(dentries + 32));  //starting address + 32B (file name) 
			dentry->inode_num = *((int32_t*)(dentries + 36));  //starting address + 32B (file name) + 4B (file type)
			return 0;
		}
		dentries += 64;   //next dentry
	}
	return -1;
}

/*
 * read_dentry_by_dir_index
 *   DESCRIPTION:	Reads a dentry based on a given directory index.
 *   INPUTS: 		index  - integer index of the directory
 *					dentry - empty dentry_t struct to fill
 *   OUTPUTS: 		None
 *   RETURN VALUE: 	1 on success, -1 on failure 
 *   SIDE EFFECTS: 	None
 */
int32_t read_dentry_by_dir_index (uint32_t index, dentry_t* dentry){

	//check for null dentry
	if(dentry == 0x0){
		return -1;
	}
	
	// Get a pointer to the dir entry
	uint32_t dentries = fs + 64 + (64*index);
	
	//copy file name character by character
	int j;
	for(j=0; j<32; j++){
		dentry->file_name[j] = *((uint32_t*)(dentries+j));
	}
	dentry->file_type = *((int32_t*)(dentries + 32));  //starting address + 32B (file name) 
	dentry->inode_num = *((int32_t*)(dentries + 36));  //starting address + 32B (file name) + 4B (file type)
	
	return 1;  //success
}

/*
 * read_data
 *   DESCRIPTION:	Reads length bytes starting at offset of the file represented by the given inode
 *   INPUTS:		inode  - index of inode representing desired file
 *					offset - byte to start reading from
 *					buf    - character buffer to fill
 *					length - number of bytes to read
 *   OUTPUTS: 		None
 *   RETURN VALUE: 	Number of bytes read, -1 indicating error, 0 indicating end of file read 
 *   SIDE EFFECTS: 	The buffer is filled with characters from file
 */
int32_t read_data (uint32_t inode, uint32_t offset, uint8_t* buf, uint32_t length){
	//check for bad inode index
	if(inode < 0 || inode > 63){
		return -1;
	}
	
	//check for null buf
	if(buf == 0x0){
		return -1;
	}

	uint32_t num_read = 0;   //number of bytes read
	int i = 0;               //current data block counter
	uint32_t num_inodes = *((uint32_t *)(fs + 4));       //number of inodes in system
	uint32_t num_data_blocks = *((uint32_t *)(fs + 8));  //number of data blocks in system
	uint32_t inode_addrs = fs + ((inode+1) * 4096);      //start of given inode 
	uint32_t data_addrs = fs + ((num_inodes+1) * 4096);  //start of data blocks
	uint32_t d = inode_addrs + 4; 						 //address of current data block
	uint32_t num_bytes = *((uint32_t *)inode_addrs);
	
	//visit every data block pointer	
	while(i <= num_bytes/4096){
		int j = 0;
		uint32_t cur_data_index = *((uint32_t *)d);                     //integer index of data block
		uint32_t cur_data_pointer = data_addrs+(cur_data_index*4096);   //address of data block
		
		//check for bad data block entry
		if(cur_data_index > num_data_blocks){
			return -1;
		}
		
		//check if offset is in this data block
		if(offset / 4096 == i){
			//cur_data_pointer += offset;
			j = offset % 4096;
		}
		//offset is past this file
		else if(offset / 4096 > i){
			i++;
			continue;
		}
		
		//current character in file
		unsigned char cur_char; //= *((uint8_t *)cur_data_pointer);
		
		//make sure number of read bytes is less than length
		while(num_read < length && j < 4096){
			 //number of bytes read so far
			//num_read++;
			cur_char = *((uint8_t *)(cur_data_pointer + j));
			
			//write character to buffer
			buf[(i*4096) + j - offset] = cur_char;
			j++;
			num_read++;
			
			if((num_read + offset) == num_bytes){
				return 0;
			}
			
			//find next character
			//cur_char = *((uint8_t *)(cur_data_pointer + j));
		}

		i++;
		
		//get address to next data block
		d = inode_addrs + 4 + (i * 4);
	}
	
	//made it to the end of the file
	return num_read;
}

/*
 * file_size
 * FUNCTION:		Calculates the size of a file.
 * INPUT:			uint32_t inode - Index that locates
 *									 the file we're looking
 *									 for.
 * OUTPUT: 			None
 * RETURN VALUE:	Returns the size of the file in bytes
 * SIDE EFFECTS:	None
 */
uint32_t file_size(uint32_t inode)
{
	uint32_t inode_addrs = fs + ((inode+1) * 4096);
	return *((uint32_t *)inode_addrs);
}

/************************************************************************************/

/*
 * fs_init
 *   DESCRIPTION:	Initialize file sytem global variables
 *   INPUTS: 		Pointer to start of file sytem
 *   OUTPUTS: 		None
 *   RETURN VALUE:	None 
 *   SIDE EFFECTS:	None
 */
void fs_init(uint32_t fs_start){
	// Get the pointer start of file system. (i.e. boot block)
	fs = fs_start;
}

/*
 * fs_open
 *   DESCRIPTION: 	Not used or implemented as file image 
 *					is already loaded.
 *   INPUTS: 		None
 *   OUTPUTS: 		None
 *   RETURN VALUE: 	-1; shouldn't be called 
 *   SIDE EFFECTS: 	None
 */
int32_t fs_open(){
	//Image already in kernel
	return -1;
}

/*
 * fs_read
 *	DESCRIPTION:	Read file specified by buf and read nbytes of that file and put into buf
 *  INPUTS:			buf -	initialized with what wants to be read (defined in filesystem.h)
							that will be overrided with the file data.
					nbytes - max number of bytes to put into buf
 *	OUTPUTS: 		None
 *	RETURN VALUE: 	Number of bytes read (0 if file not found)
 *	SIDE EFFECTS: 	The buffer is filled with the file data 
 */
int32_t fs_read(void* buf, int32_t nbytes){

	//Check for a valid buf
	if (buf == NULL)
		return 0;
	
	dentry_t entry;
	
	//Check what buf wants to read
	uint8_t *fname;
	uint32_t index;
	uint32_t offset = 0;
	switch (*((uint32_t *)buf))
	{
		case 0: //Read file by name
			fname = (uint8_t *)(buf + 4);
			if ( read_dentry_by_name (fname, &entry) )
			{
				//File not found!!!
				return 0;
			}
			break;
		case 1: //Read file by index
			index = *((uint32_t *)(buf + 4));
			offset = *((uint32_t *)(buf + 8));
			if ( read_dentry_by_index (index, &entry))
			{
				//File not found!!!
				return 0;
			}
			break;
	}
	
	//Read data if buf is big enough for the file
	uint32_t fsize = *((uint32_t *)(fs + ((entry.inode_num + 1) * 4096))); // get inode num bytes for file
	
	int retval = read_data (entry.inode_num, offset, (uint8_t *)buf, nbytes);
	
	if ( retval == 0 )
	{
		//End of file reached so nbytes read
		return fsize;
	}

	return retval;
}

/*
 * fs_write
 *   DESCRIPTION:	Does nothing, returns -1 to imply that files can't be written to
 *   INPUTS: 		Pointer to a buffer, max number of bytes to put in the buffer
 *   OUTPUTS: 		None
 *   RETURN VALUE:	-1 (This is a read-Only file system)
 *   SIDE EFFECTS:	None
 */
int32_t fs_write(const void* buf, int32_t nbytes){
	//Read only file system
	return -1;
}

/*
 * fs_close
 *   DESCRIPTION:	Not implemented as you cannot close the only file system.
 *   INPUTS:		None
 *   OUTPUTS: 		None
 *   RETURN VALUE:	-1
 *   SIDE EFFECTS:	None
 */
int32_t fs_close(){
	//Can't close the only file system
	return -1;
}

/*
 * print_dentry
 *   DESCRIPTION:	Prints dentry information to display
 *   INPUTS:		entry - dentry_t structure we want to print
 *   OUTPUTS: 		None
 *   RETURN VALUE:	None 
 *   SIDE EFFECTS: 	Prints to screen
 */
void print_dentry(dentry_t entry){
	printf("\nname : ");
	int pos;
	for(pos=0; pos<32; pos++){
		printf("%c",entry.file_name[pos]);
	}
	printf("\ntype : %d", entry.file_type);
	printf("\ninode: %d\n", entry.inode_num);
}

/*
 * is_valid_cmd
 *  DESCRIPTION:	Checks if given program name is a valid executable
 *  INPUTS:			executable 	 - pointer of dentry to fill
					program_name - name of program to load 
 *  OUTPUTS:		None
 *	RETURN VALUE:	-1 if program does not exist, 1 otherwise
 *  SIDE EFFECTS:	Fills executable (argument passed in)
 */
int32_t is_valid_cmd(dentry_t * executable, const uint8_t* program_name){
	if (read_dentry_by_name(program_name, executable) == -1)
	{
		//Not valid file name
		return -1;
	}
	//Check if file is exe (ELF magic number is 0x464C457F)
	uint8_t buf[4];
	read_data(executable->inode_num, 0, buf, 4);
	if (*((uint32_t *)buf) != 0x464C457F)
	{
		//Not an executable
		return -1;
	}
	return 1;
}

/*
 * load_program
 *   DESCRIPTION:	Loads program into memory
 *   INPUTS: 		program_name - name of program to load
 *					esp - Location to store value of %ESP 
 *					eip - Location to store value of %EIP
 *					pid - Process ID
 *   OUTPUTS: 		None
 *   RETURN VALUE: 	Returns a pcb_t pointer, filled with relevant
 *					information to load the file. If the command is
 *					invalid, it returns a NULL pointer. 
 *   SIDE EFFECTS: 	None
 */
pcb_t *load_program(const uint8_t* program_name, uint32_t *esp, uint32_t *eip, int pid){
	uint8_t *program_mem;
	pcb_t * pcb;
	
	//only load if program is valid and executable
	dentry_t executable;
	if(is_valid_cmd(&executable, program_name) == -1){
		return NULL;
	}
	
	//Load file into memory
	program_mem = (uint8_t *)(_128MB + EXE_OFFSET);  //128MB virtual
	pcb = (pcb_t *)(_8MB - (pid*_8KB));   			//pcb located at (bottom 8KB * process id) of kernel 
	pcb -> pid = pid;
	
	int i;
	for (i = 0; i < _4MB - EXE_OFFSET; i++){
		*(program_mem + i) = 0x00;
	}
	
	uint32_t f_size = file_size(executable.inode_num);
	read_data(executable.inode_num, 0, program_mem, f_size);
	
	//Set EIP
	*eip = *((uint32_t *)(program_mem + 24));
	
	//Set ESP
	*esp = (uint32_t)(program_mem - EXE_OFFSET + _4MB);
	
	return pcb;
}

