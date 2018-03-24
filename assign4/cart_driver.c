////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_driver.c
//  Description    : This is the implementation of the standardized IO functions
//                   for used to access the CRUD storage system.
//
//  Author         : Edward Bagdon
//  Last Modified  : 10/24/16
//

// Includes
#include <stdlib.h>

// Project Includes
#include <cart_driver.h>
#include <cart_controller.h>
#include <cart_support.h>
#include <string.h>
#include <cmpsc311_log.h>
#include <cart_cache.h>
#include <cart_network.h>

// Implementation
// Global Variables
uint16_t NextFrame; 				//global int that contains the location of the next frame to be allocated
									//The upper 6 bits of this variable contain the cartridge number..
									//while the lower 10 contain the frame

uint16_t FileCounter;				//next file handle to be assigned
CartridgeIndex CurrentCart;			//Number of the current cartridge loaded
file *files;						//global pointer to the first file in the file structure

int cachehits;
int cachemisses;
////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_poweron
// Description  : Startup up the CART interface, initialize filesystem
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t cart_poweron(void) {

	char KY1, KY2, RT;
	uint16_t CT1, FM1;
	NextFrame = 0;		//Initalize global variables and data structures
	FileCounter = 0;
	CurrentCart = 0;
	cachehits = 0;
	cachemisses = 0;
	files = calloc(CART_MAX_TOTAL_FILES , sizeof(file));
	
	CartXferRegister INIT, RESP, LDCART, ZEROCART;
	
	INIT = create_cart_opcode(CART_OP_INITMS,0,0,0);
	RESP = client_cart_bus_request(INIT, NULL);
	
	extract_cart_opcode(RESP, &KY1, &KY2, &RT, &CT1, &FM1);

	if(RT !=0){
		logMessage(LOG_ERROR_LEVEL,"CART INITIALIZATION FAILED");
		return (-1);
	}
	int i;
	for(i = 0;i<CART_MAX_CARTRIDGES;i++){
	
	LDCART = create_cart_opcode(CART_OP_LDCART,0,i,0);
	client_cart_bus_request(LDCART,NULL);
	
	ZEROCART = create_cart_opcode(CART_OP_BZERO, 0, i,0);
	client_cart_bus_request(ZEROCART, NULL);
	}
	
	LDCART = create_cart_opcode(CART_OP_LDCART,0,0,0);
	client_cart_bus_request(LDCART,NULL);

	init_cart_cache();
	// Return successfully
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_poweroff
// Description  : Shut down the CART interface, close all files
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t cart_poweroff(void) {
	char RT, KY1, KY2;
	uint16_t CT1, FM1;
	CartXferRegister SHUTDOWN,RESP;
	SHUTDOWN = create_cart_opcode(CART_OP_POWOFF, 0,0,0);
	
	RESP = client_cart_bus_request(SHUTDOWN, NULL);

	extract_cart_opcode(RESP, &KY1, &KY2, &RT, &CT1, &FM1);

	if(RT !=0){
		logMessage(LOG_ERROR_LEVEL,"CART POWEROFF FAILED");
		return (-1);
	}

	logMessage(LOG_OUTPUT_LEVEL, "\nCache Hits:%d\nCache Misses:%d\n", cachehits, cachemisses);
	// Return successfully
	close_cart_cache();
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_open
// Description  : This function opens the file and returns a file handle
//
// Inputs       : path - filename of the file to open
// Outputs      : file handle if successful, -1 if failure

int16_t cart_open(char *path) {

	int i = 0;
	
	while(strcmp(path ,files[i].path) == 0 || i < FileCounter){ 
		i++;
	}
	if(strcmp(path ,files[i].path) == 0){		//If a file with path name already exists
		if(files[i].status == OPEN ){			//Check if the file is already open
			logMessage(LOG_ERROR_LEVEL, "Error: File already open \n");
			return(-1);							//Return -1 if it is already open
		}
		files[i].status = OPEN;					//If the file is not open, set file to OPEN
							
	}
	if(i >= FileCounter){						//If the end of the table is reached without finding the path
		strncpy( files[i].path, path, strlen(path));//Create a new file
		files[i].status = OPEN;					//Set file to open
		files[i].fd = i+1;						//Assign a file handle
		FileCounter++;							//Increase the file count by one for the new file	
	}						
	
	return (files[i].fd);						//Return the file handle
	
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_close
// Description  : This function closes the file
//
// Inputs       : fd - the file descriptor
// Outputs      : 0 if successful, -1 if failure

int16_t cart_close(int16_t fd) {	
	if(fd > FileCounter || fd < 1){
		logMessage(LOG_ERROR_LEVEL, "Error: Bad file handle \n");
		return(-1);			//Failure: bad file handle
	}
	if(files[fd-1].status == CLOSED){
		logMessage(LOG_ERROR_LEVEL, "Error: File already closed \n");
		return(-1);			//Failure: file already closed
	}
	files[fd-1].status = CLOSED;

	return (0);				// Return successfully
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_read
// Description  : Reads "count" bytes from the file handle "fh" into the 
//                buffer "buf"
//
// Inputs       : fd - filename of the file to read from
//                buf - pointer to buffer to read into
//                count - number of bytes to read
// Outputs      : bytes read if successful, -1 if failure

int32_t cart_read(int16_t fd, void *buf, int32_t count) {
	
	if(fd > FileCounter || fd < 1){
		logMessage(LOG_ERROR_LEVEL, "Error: Bad file handle\n");
		return(-1);			//Failure: bad file handle
	}
	if(files[fd-1].status == CLOSED){
		logMessage(LOG_ERROR_LEVEL, "Error: File not open \n");
		return(-1);			//Failure: file not open
	}
	
	file *rfile = &files[fd-1];
	//Declare buffers
	char* rbuf = calloc(count,sizeof(char));
	char* framebuf = calloc(CART_FRAME_SIZE, sizeof(char));
	char* cachebuf;

	int32_t byteOffset = rfile ->fp % CART_FRAME_SIZE;
	int FrameIndex = rfile->fp/CART_FRAME_SIZE;
	int32_t BytesToRead = min(count,(rfile -> filesize - rfile -> fp));
	//Set bytes to read to either count or the amount of bytes from fp to the end of the file
	count = BytesToRead;
	uint16_t FM1, CT1;
	CartXferRegister READ, LOADCART;
	int i = 0;

	if (byteOffset !=0){
		file_ExtractFrame(rfile -> CartFrame[FrameIndex], &FM1, &CT1);
		cachebuf = get_cart_cache(CT1, FM1); 
		if(cachebuf == NULL){										//If the frame is not on the cache read normally
		cachemisses++;
		if (CT1 != CurrentCart){									//If the cartridge requested is not the current cartridge
		LOADCART = create_cart_opcode(CART_OP_LDCART,0 ,CT1,0);		//Load requested cartridge
		client_cart_bus_request(LOADCART,NULL);
		CurrentCart = CT1;
		}

		READ = create_cart_opcode(CART_OP_RDFRME,0,CT1,FM1);
		client_cart_bus_request(READ,framebuf);
		
		put_cart_cache(CT1, FM1, framebuf);							//put the frame on the cache
		
		memcpy(rbuf,&framebuf[byteOffset],min((CART_FRAME_SIZE - byteOffset),(BytesToRead)));
		}
		else{
			cachehits++;
			memcpy(rbuf,&cachebuf[byteOffset],min((CART_FRAME_SIZE - byteOffset),(BytesToRead)));
		}

		BytesToRead = BytesToRead - min((CART_FRAME_SIZE - byteOffset),(BytesToRead));
		FrameIndex++;
		i++;
	}

	
	while(BytesToRead  >0){

		file_ExtractFrame(rfile -> CartFrame[FrameIndex], &FM1, &CT1);
		cachebuf = get_cart_cache(CT1, FM1);
		if (cachebuf == NULL){
		cachemisses++;
		if (CT1 != CurrentCart){									//If the cartridge requested is not the current cartridge
		LOADCART = create_cart_opcode(CART_OP_LDCART,0 ,CT1,0);		//Load requested cartridge
		client_cart_bus_request(LOADCART,NULL);
		CurrentCart = CT1;
		}

		READ = create_cart_opcode(CART_OP_RDFRME,0,CT1,FM1);
		client_cart_bus_request(READ,framebuf);									//Send read message over bus
		put_cart_cache(CT1, FM1, framebuf);
		memcpy(&rbuf[(i*CART_FRAME_SIZE - byteOffset)],framebuf,min(CART_FRAME_SIZE,BytesToRead));
		}
		
		else{
			cachehits++;
			memcpy(&rbuf[(i*CART_FRAME_SIZE - byteOffset)],cachebuf,min(CART_FRAME_SIZE,BytesToRead));
		}
		
		
		BytesToRead = BytesToRead - min(CART_FRAME_SIZE, BytesToRead);//update bytes to read by subtracting 1024 or bytes to read

		FrameIndex++;												//Move to the next frame
		i++;														//increment i for address purposes
	}
	memcpy(buf,rbuf,count);
	rfile -> fp = rfile ->fp +count;
	// Return successfully
	free(rbuf);
	free(framebuf);
	return (count);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_write
// Description  : Writes "count" bytes to the file handle "fh" from the 
//                buffer  "buf"
//
// Inputs       : fd - filename of the file to write to
//                buf - pointer to buffer to write from
//                count - number of bytes to write
// Outputs      : bytes written if successful, -1 if failure

int32_t cart_write(int16_t fd, void *buf, int32_t count) {

	CartXferRegister WRITE, READ, LOADCART;
	
	if(fd > FileCounter || fd < 1){ 		//Check File handle
		logMessage(LOG_ERROR_LEVEL, "Error: Bad file handle. \n");
		return(-1);						//Failure: bad file handle
		}						
	if(files[fd-1].status == CLOSED){	//Check of the file is open
		logMessage(LOG_ERROR_LEVEL, "Error: File not open. \n");
		return(-1);						//Failure: file not open
		}
	file *wfile;
	wfile = &files[fd-1];

	uint16_t CT1, FM1;
	char *mbuf = calloc(count, sizeof(char));
	char *writebuf = calloc(CART_FRAME_SIZE, sizeof(char));
	
	uint32_t FramesToAllocate;
	int i= 0;
	int32_t BytesToWrite = count;
	
	int FrameIndex = (wfile->fp)/CART_FRAME_SIZE;
	int32_t byteOffset = (wfile->fp) % CART_FRAME_SIZE;
	 
	if(wfile->fp + count > wfile->filesize)
		wfile->filesize = wfile->fp +count;

	FramesToAllocate = ((wfile -> filesize)/CART_FRAME_SIZE) +1 - (wfile -> NumberOfFrames); 

	memcpy(mbuf, buf, count);

	while(i < FramesToAllocate){	
		AllocateFrame( wfile);	//allocates the amount of frames needed
		i++;
	}

	i = 0;

	if(byteOffset !=0){					//for "incomplete frames" 

		file_ExtractFrame(wfile -> CartFrame[FrameIndex], &FM1, &CT1); //Find the correct frame	

		if (CT1 != CurrentCart){									//If the cartridge requested is not the current cartridge
		LOADCART = create_cart_opcode(CART_OP_LDCART,0 ,CT1,0);		//Load requested cartridge
		client_cart_bus_request(LOADCART,NULL);
		CurrentCart = CT1;
		}
	
		READ = create_cart_opcode(CART_OP_RDFRME,0,CT1,FM1);		//Cart opcode to read from the incomplete frame
		client_cart_bus_request(READ,writebuf);

		memcpy(&writebuf[byteOffset],mbuf, min(BytesToWrite,CART_FRAME_SIZE - byteOffset)); //Copy the bytes from buf to writebuf
		//The number of bytes written is either count or the remainder of the frame to be written, whichever is less

		WRITE = create_cart_opcode(CART_OP_WRFRME,0, CT1, FM1);		//Write to the frame
		client_cart_bus_request(WRITE,writebuf);
		put_cart_cache(CT1,FM1,writebuf);

		BytesToWrite = BytesToWrite - min(CART_FRAME_SIZE - byteOffset, BytesToWrite); //Update the bytes to be written
		
		FrameIndex++; //Move to the next frame in the file
		i =1;			//Set i equal to one for offset purposes
		
	}
	

	while(BytesToWrite > 0){	//for "empty frames"
		
		file_ExtractFrame(wfile -> CartFrame[FrameIndex], &FM1, &CT1);				//extract cart and frame numbers
		
		if (CT1 != CurrentCart){										// Load new cartridge if needed
		LOADCART = create_cart_opcode(CART_OP_LDCART,0 ,CT1,0);
		client_cart_bus_request(LOADCART,NULL);
		CurrentCart = CT1;
		}
		READ = create_cart_opcode(CART_OP_RDFRME,0,CT1,FM1);			//Read frame to prevent complete overwrite
		client_cart_bus_request(READ,writebuf);
		memcpy(writebuf,&mbuf[(i*CART_FRAME_SIZE)-byteOffset], min(BytesToWrite, CART_FRAME_SIZE)); //either copies an entire frame
		// or BytesToWrite

		WRITE = create_cart_opcode(CART_OP_WRFRME,0, CT1, FM1); 		//Send Write opcode
		client_cart_bus_request(WRITE,writebuf);
		put_cart_cache(CT1,FM1,writebuf);

		BytesToWrite = BytesToWrite - min(CART_FRAME_SIZE, BytesToWrite); // Update the BytesToWrite
		
		FrameIndex++;// move to the next frame
		i++;			//Increment i to modify the address of the source mbuf in memcpy
	}

	wfile->fp  = wfile -> fp +count;
	
	free(mbuf);
	free(writebuf);

	return (count);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_read
// Description  : Seek to specific point in the file
//
// Inputs       : fd - filename of the file to write to
//                loc - offfset of file in relation to beginning of file
// Outputs      : 0 if successful, -1 if failure

int32_t cart_seek(int16_t fd, uint32_t loc) {
	
	if(fd > FileCounter || fd < 1){
		logMessage(LOG_ERROR_LEVEL, "Error: Bad file handle. \n");
		return(-1);									//Failure: bad file handle
	}
	if (files[fd-1].status == CLOSED){
		logMessage(LOG_ERROR_LEVEL, "Error: File not open. \n");
		return(-1);									//Failure: file not open
	}
	if (loc > files[fd-1].filesize){				//if the loc is greater than the total filesize
		files[fd-1].fp = files[fd-1].filesize;		//set file pointer to the end of the file
		return(0);
	}													
	files[fd-1].fp = loc;
	// Return successfully
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : create_cart_opcode
// Description  : Create a packed register from KY1, KY2
//
// Inputs       : KY1- Key Register 1, KY2- Key Register 2, CT1- Cart Number
//                FM1- Frame Number
// Outputs      : packed_opcode- a 64 bit opcode to send over the I/O bus

CartXferRegister create_cart_opcode(uint64_t KY1, uint64_t KY2, uint64_t CT1, uint64_t FM1){
	CartXferRegister packed_opcode;
	uint64_t tmp_KY1, tmp_KY2, tmp_CT1, tmp_FM1;
				
	tmp_KY1 = KY1;
	tmp_KY2 = KY2;
	tmp_CT1 = CT1;
	tmp_FM1 = FM1;

	tmp_KY1 = tmp_KY1 << 56;
	tmp_KY2	= tmp_KY2 << 48;
	tmp_CT1	= tmp_CT1 << 31;
	tmp_FM1	= tmp_FM1 << 15;

	packed_opcode = tmp_KY1|tmp_KY2|tmp_CT1|tmp_FM1;		
		
	return (packed_opcode); 
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : extract_cart_opcode
// Description  : extract register values from the 64-bit return opcode
//
// Inputs       : KY1- Key Register 1, KY2- Key Register 2, CT1- Cart Number
//                FM1- Frame Number, RT- Return Bit
//
// Outputs      : 0 if successful, -1 if failure.  
//				  Also the register values KY1, KY2, RT, CT1, and FM1 which are passed by reference

int16_t extract_cart_opcode(CartXferRegister resp, char *KY1, char *KY2, char *RT, uint16_t *CT1, uint16_t *FM1){
	
	uint64_t tmp_KY1, tmp_KY2, tmp_CT1, tmp_FM1, tmp_RT; //Temp variables
	
	tmp_KY1 = resp & KY1_MASK;	// Mask the bits for each register
	tmp_KY2 = resp & KY2_MASK;
	tmp_RT  = resp & RT_MASK;				
	tmp_FM1 = resp & FM1_MASK;
	tmp_CT1 = resp & CT1_MASK;	

	tmp_KY1 = tmp_KY1 >> 56;	//Shift bits for each register
	tmp_KY2 = tmp_KY2 >> 48;
	tmp_RT  = tmp_RT  >> 47;
	tmp_CT1 = tmp_CT1 >> 31;
	tmp_FM1 = tmp_FM1 >> 15;

	*KY1 = (char)tmp_KY1;		//Cast the register values to the appropriate type
	*KY2 = (char)tmp_KY2;
	*RT  = (char)tmp_RT; 
	*CT1 = (uint16_t)tmp_CT1;
	*FM1 = (uint16_t)tmp_FM1;

	// Return successfully
	return (0);	
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : file_ExtractFrame
// Description  : extract cartridge and frame values from location
//
// Inputs       : By reference: frame, cart
//                
// Outputs      : 0 if successful, frame number and cartridge number

int32_t file_ExtractFrame(uint16_t location, uint16_t* frame, uint16_t *cartridge){
	*frame = location & 0x03FF;
	*cartridge  = location >> 10;

	return(0);
}
////////////////////////////////////////////////////////////////////////////////
//
// Function     : Allocate Frame
// Description  : allocates a frame for file struct passed by reference
//
// Inputs       : A pointer to struct file
//                
// Outputs      : 0 if successful, (By reference) struct file

int16_t AllocateFrame(file *file){
	int i = file->NumberOfFrames;	//sets i to the current number of frames in the file
	if (i==0){
		file-> CartFrame = malloc((i+1)*sizeof(uint16_t));
		file->CartFrame[i] = NextFrame;	//the file's frame araay is updated with the next frame available			
	}
	else{
		file->CartFrame = realloc(file->CartFrame,(i+1)*sizeof(uint16_t));
		file ->CartFrame[i] = NextFrame;
	}					
		file -> NumberOfFrames++;
		NextFrame++;
		//the file's number of frames are updated
	return (0);

}
////////////////////////////////////////////////////////////////////////////////
//
// Function     : min
// Description  : returns the minimum of two values passed, used to determine the "n" bytes in memcpy()
// Inputs       : Two 32-bit integers         
// Outputs      : Lowest of the 2 integers

int32_t min(int32_t a, int32_t b){
	if(a < b)
		return a;
	return b;
}
