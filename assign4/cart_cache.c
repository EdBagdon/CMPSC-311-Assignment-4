////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_cache.c
//  Description    : This is the implementation of the cache for the CART
//                   driver.
//
//  Author         : Edward Bagdon
//  Last Modified  : 11/18/16
//

// Includes
#include <stdlib.h>
#include <string.h>
// Project includes
#include <cache_support.h>
#include <cmpsc311_log.h>
#include <cmpsc311_util.h>
// Defines
#define CART_FRAME_SIZE 1024
//Global Variables
uint32_t maxFrames;
cache_entry **cacheEntries;

// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : set_cart_cache_size
// Description  : Set the size of the cache (must be called before init)
//
// Inputs       : max_frames - the maximum number of items your cache can hold
// Outputs      : 0 if successful, -1 if failure

int set_cart_cache_size(uint32_t max_frames) {
maxFrames = max_frames;
return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : init_cart_cache
// Description  : Initialize the cache and note maximum frames
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int init_cart_cache(void) {
cacheEntries = calloc(maxFrames,sizeof(void *));
return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : close_cart_cache
// Description  : Clear all of the contents of the cache, cleanup
//
// Inputs       : none
// Outputs      : o if successful, -1 if failure

int close_cart_cache(void) {
int i;
for(i=0;i<maxFrames;i++){
	free(cacheEntries[i]);
}
free(cacheEntries);
return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : put_cart_cache
// Description  : Put an object into the frame cache
//
// Inputs       : cart - the cartridge number of the frame to cache
//                frm - the frame number of the frame to cache
//                buf - the buffer to insert into the cache
// Outputs      : 0 if successful, -1 if failure

int put_cart_cache(CartridgeIndex cart, CartFrameIndex frm, void *buf)  {
cache_entry *putCache;	
putCache = malloc(sizeof(cache_entry));
putCache -> cart = cart;
putCache -> frm = frm;
putCache -> LastUse = 0;
memcpy(putCache -> framebuf, buf, CART_FRAME_SIZE);
int i=0;
int j=0;
int LRU=0;

while(cacheEntries[i] != NULL && i< maxFrames){
	if(cacheEntries[i]->frm == frm && cacheEntries[i]-> cart == cart ){	//if the frame is already in the cache replace it
		break;
	}
	if(cacheEntries[i]-> LastUse > LRU){								//find the least recently used
		LRU = cacheEntries[i]-> LastUse;
		j = i;
	}
	i++;																
}
if(i == maxFrames){			//If the cache is full, replace the LRU frame
	free(cacheEntries[j]);
	cacheEntries[j] = putCache;
}
else {						//If the cache is not full, place the frame in the next available entry
	free(cacheEntries[i]);
	cacheEntries[i] = putCache;
}


return(0);

}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_cart_cache
// Description  : Get an frame from the cache (and return it)
//
// Inputs       : cart - the cartridge number of the cartridge to find
//                frm - the  number of the frame to find
// Outputs      : pointer to cached frame or NULL if not found

void * get_cart_cache(CartridgeIndex cart, CartFrameIndex frm) {
	void *frameptr = NULL;
	int i = 0;
	while(cacheEntries[i] != NULL && i < maxFrames){
		if (cacheEntries[i]->frm == frm && cacheEntries[i]->cart == cart){
			cacheEntries[i] -> LastUse = 0;
			frameptr = cacheEntries[i]->framebuf;
		}
		else
			cacheEntries[i] -> LastUse++;
		i++;
	}

	return(frameptr);
}


// Unit test

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cartCacheUnitTest
// Description  : Run a UNIT test checking the cache implementation
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int cartCacheUnitTest(void) {
	logMessage(LOG_OUTPUT_LEVEL, "Initializing Cache");
	set_cart_cache_size(100);
	init_cart_cache();
	int i;
	CartFrameIndex frm1;
	CartridgeIndex cart1;
	for(i=0;i<200;i++){
		char framebuf[1024];
		getRandomData(framebuf, 1024);
		CartridgeIndex cart = getRandomValue(0, 63);
		CartFrameIndex frm = getRandomValue(0,1023);
		logMessage(LOG_OUTPUT_LEVEL, "Putting Frame:%d , Cart:%d on the cache \n",frm, cart);
		
		put_cart_cache(cart, frm, framebuf);
		frm1 = frm;
		cart1 = cart;
	}
char* membuf;
		membuf = get_cart_cache(cart1, frm1);

	

	// Return successfully
	logMessage(LOG_OUTPUT_LEVEL, "Cache unit test completed successfully.");
	return(0);
}
