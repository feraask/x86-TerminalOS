/* 
	Header for sys_call_handler.S
*/

#ifndef _SYS_CALL_HANDLER_H
#define _SYS_CALL_HANDLER_H

#include "sys_calls.h"

#ifndef ASM
extern int32_t sys_call_handler();

#endif
#endif
