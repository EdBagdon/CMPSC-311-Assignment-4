#ifndef CART_SUPPORT_INCLUDED
#define CART_SUPPORT_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_support.h
//  Description    : This is the header file for support functions, definitions, and data structures for the cart_driver
//
//  Author         : Edward Bagdon
//  Last Modified  : 10/24/16
//
#include <stdint.h>
#include <cart_support.h>
#include <cart_controller.h>

//Define masks for unpacking registers
#define KY1_MASK 0xFF00000000000000
#define KY2_MASK 0x00FF000000000000	
#define RT_MASK	0x0000800000000000
#define CT1_MASK 0x00007FFF80000000
#define	FM1_MASK 0x000000007FFF8000

//The Main file structure
typedef struct {
	char path[128];						//File path  
	int16_t fd;							//File descriptor
	int32_t fp;							//File pointer in number of bytes
	int32_t filesize;					//File Size in bytes					
	int NumberOfFrames;					//Number of frames allocated for the file
	uint16_t *CartFrame;			//Locations of frames the file is stored in
										//The upper 6 bits of this variable contain the cartridge number..
										//while the lower 10 contain the frame number
	enum{
		CLOSED = 0,
		OPEN = 1
	}status;							//Enum for whether the file is currently opened or closed

}file;


CartXferRegister create_cart_opcode(uint64_t KY1, uint64_t KY2, uint64_t CT1, uint64_t FM1);
//Creates a packed register using shifts and masks

int16_t extract_cart_opcode(CartXferRegister resp, char *KY1, char *KY2, char *RT, uint16_t *CT1, uint16_t *FM1);
//extracts values and places them in the function parameters 

int32_t file_ExtractFrame(uint16_t location,uint16_t* frame, uint16_t *cart);
//extracts a cartridge and frame value from location

int16_t AllocateFrame(file* file);
//Allocates an addtional frame to the file

int32_t min(int32_t a, int32_t b);
//returns the minimum value of a and b

#endif


