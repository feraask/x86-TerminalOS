/* directory.c
 * Driver for the directory
 */
 
#include "directory.h"

uint32_t num_entries = 0;
int32_t cur_entry = 0;

/*
 * dir_open
 *   DESCRIPTION:	Opens a new directory,
 *					initializing relevant values
 *   INPUTS:		None
 *   OUTPUTS:		None
 *   RETURN VALUE:	Returns 1
 *   SIDE EFFECTS:	None
 */
int32_t dir_open(){
	num_entries = num_dir_entries();
	cur_entry = 0;
	return 1;
}

/*
 * dir_read
 *   DESCRIPTION:	Reads a directory entry
 *   INPUTS:		Pointer to the buffer and size of buffer
 *   OUTPUTS:		None
 *   RETURN VALUE:	0 if all directories have been read, 
 *					otherwise returns length of file name
 *					corresponding to the current directory
 *					entry.
 *   SIDE EFFECTS:	Copies file name into the buffer,
 *					changes the current directory
 */
int32_t dir_read(void* buf, int32_t nbytes){
	if(cur_entry >= num_entries){
		return 0;    //already read all directory entries
	}
	
	//get cur_entry
	dentry_t entry;
	read_dentry_by_dir_index(cur_entry, &entry);
	
	//fill buf with dentry name
	strncpy(buf, entry.file_name, 32);
	
	cur_entry++;
	return strlen(entry.file_name);
}

/*
 * dir_write
 *   DESCRIPTION:	Not used or implemented.
 *   INPUTS: 		None
 *   OUTPUTS:		None
 *   RETURN VALUE: 	Returns -1 as it should not be called.
 *   SIDE EFFECTS:	None
 */
int32_t dir_write(const void* buf, int32_t nbytes){
	return -1;
}

/*
 * dir_close
 *   DESCRIPTION:	Not used or implemented.
 *   INPUTS: 		None
 *   OUTPUTS:		None
 *   RETURN VALUE: 	Returns -1 as it should not be called.
 *   SIDE EFFECTS:	None
 */
int32_t dir_close(){
	return -1;
}
