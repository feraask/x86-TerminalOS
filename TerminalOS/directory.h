/* directory.h
 * Header for directory driver
 */
 
#include "lib.h"
#include "filesystem.h"

int32_t dir_open();
int32_t dir_read(void* buf, int32_t nbytes);
int32_t dir_write(const void* buf, int32_t nbytes);
int32_t dir_close();
