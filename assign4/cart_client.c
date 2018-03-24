////////////////////////////////////////////////////////////////////////////////
//
//  File          : cart_client.c
//  Description   : This is the client side of the CART communication protocol.
//
//   Author       : Edward Bagdon
//  Last Modified : 12/9/16
//

// Include Files
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <cmpsc311_util.h>
#include <stdlib.h>
#include <unistd.h>

// Project Include Files
#include <cart_network.h>
#include <cart_controller.h>
#include <cmpsc311_log.h>
#include <cart_support.h>
//
//  Global data
int client_socket = -1;
int                cart_network_shutdown = 0;   // Flag indicating shutdown
unsigned char     *cart_network_address = NULL; // Address of CART server
unsigned short     cart_network_port = 0;       // Port of CART serve
unsigned long      CartControllerLLevel = 0; // Controller log level (global)
unsigned long      CartDriverLLevel = 0;     // Driver log level (global)
unsigned long      CartSimulatorLLevel = LOG_INFO_LEVEL;  // Driver log level (global)

struct sockaddr_in cart_sock;


//
// Functions
int16_t extract_cart_opcode(CartXferRegister resp, char *KY1, char *KY2, char *RT, uint16_t *CT1, uint16_t *FM1);
//
int16_t client_test(void);
//
CartXferRegister client_read(CartXferRegister reg, void* buf);
//
CartXferRegister client_write(CartXferRegister reg, void *buf);
//
CartXferRegister client_poweroff(CartXferRegister reg);
//
CartXferRegister client_op(CartXferRegister reg);

////////////////////////////////////////////////////////////////////////////////
//
// Function     : client_cart_bus_request
// Description  : This the client operation that sends a request to the CART
//                server process.   It will:
//
//                1) if INIT make a connection to the server
//                2) send any request to the server, returning results
//                3) if CLOSE, will close the connection
//
// Inputs       : reg - the request reqisters for the command
//                buf - the block to be read/written from (READ/WRITE)
// Outputs      : the response structure encoded as needed

CartXferRegister client_cart_bus_request(CartXferRegister reg, void *buf) {

char KY1, KY2, RT;
uint16_t CT1, FM1;
CartXferRegister resp;

extract_cart_opcode(reg, &KY1, &KY2, &RT, &CT1, &FM1);

if(client_socket == -1){
	
	//Get Address
	cart_sock.sin_family = AF_INET;
	cart_sock.sin_port = htons(CART_DEFAULT_PORT);
	if ( inet_aton(CART_DEFAULT_IP, &cart_sock.sin_addr) == 0 ) {
		return( -1 );
		}
	//Create Socket
	client_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (client_socket == -1) {
		return( -1 );
		}	
	//Open Connection
	if ( connect(client_socket, (const struct sockaddr *)&cart_sock, sizeof(cart_sock)) == -1 ) {
		return( -1 );
		}
	}
	if(KY1 >= CART_OP_MAXVAL){
		return(-1);
	}
	if(KY1 == CART_OP_WRFRME){
		resp = client_write(reg, buf);
	}
	if(KY1 == CART_OP_RDFRME){
		resp = client_read(reg, buf);
	}
	if(KY1 == CART_OP_POWOFF){
		resp = client_poweroff(reg);
	}
	else{
		resp = client_op(reg);
	}

	return resp;
}
////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////
int16_t client_test(void){

return(0);
}
////////////////////////////////////////////////////////////////////////////////////
CartXferRegister client_read(CartXferRegister reg, void* buf){

reg = htonll64(reg);
CartXferRegister resp;
if(write( client_socket, &reg, sizeof(reg)) != sizeof(reg)){
	return(-1);
}
read( client_socket, &resp, sizeof(resp));
read( client_socket, (char *)buf, CART_FRAME_SIZE );

return(resp);
}
////////////////////////////////////////////////////////////////////////////////////
CartXferRegister client_write(CartXferRegister reg, void *buf){

reg = htonll64(reg);
CartXferRegister resp;

if(write( client_socket, &reg, sizeof(reg)) != sizeof(reg)){
	return(-1);
}
write( client_socket, (char *)buf, CART_FRAME_SIZE );
read( client_socket, &resp, sizeof(resp));

return(resp);

}
////////////////////////////////////////////////////////////////////////////////////
CartXferRegister client_poweroff(CartXferRegister reg){

reg = htonll64(reg);
CartXferRegister resp;

if(write( client_socket, &reg, sizeof(reg)) != sizeof(reg)){
	return(-1);
}
read( client_socket, &resp, sizeof(resp));

close(client_socket); 

return(resp);
}
////////////////////////////////////////////////////////////////////////////////////
CartXferRegister client_op(CartXferRegister reg){

reg = htonll64(reg);
CartXferRegister resp;

if(write( client_socket, &reg, sizeof(reg)) != sizeof(reg)){
	return(-1);
}
read( client_socket, &resp, sizeof(resp));

return(resp);

}

