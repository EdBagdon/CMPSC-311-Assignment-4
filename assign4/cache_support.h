#ifndef CACHE_SUPPORT_INCLUDED
#define CACHE_SUPPORT_INCLUDED


////////////////////////////////////////////////////////////////////////////////
//
//  File           : cache_support.h
//  Description    : This is the header file for support functions, definitions, and data structures for cart_cache.c
//
//  Author         : Edward Bagdon
//  Last Modified  : 10/24/16
//
#include <cart_controller.h>

//Data Structure for a cache entry
typedef struct {
	char framebuf[1024];
	CartridgeIndex cart;
	CartFrameIndex frm;
	int LastUse;
}cache_entry;




#endif