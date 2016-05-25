/* keyboard.h
 *
 */
 
#include "lib.h"
#include "terminal.h" 
#include "i8259.h"
 
 // Initialize the keyboard
 void init_keyboard();
 
 // Gives proper command to terminal based on keyboard
 void print_scancode(uint16_t scancode);
