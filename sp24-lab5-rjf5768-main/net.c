#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"


/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n (len) bytes from fd; returns true on success and false on failure. 
It may need to call the system call "read" multiple times to reach the given size len. 
*/
static bool nread(int fd, int len, uint8_t *buf) {
  //count how much has been read
  int has_Read = 0;
  //read until len
  while (has_Read < len){ 
    //read from buffer
    int result = read(fd, &buf[has_Read], len - has_Read); 
    if (result < 0){
      printf("error in nread");
      return false;
    }else{
      //updates how much we read
      has_Read += result; 
    }
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on failure 
It may need to call the system call "write" multiple times to reach the size len.
*/
static bool nwrite(int fd, int len, uint8_t *buf) {
  //count how much has been write
  int has_Written = 0; 
  //write until len
  while(has_Written < len){ 
    //write from buffer
    int result = write(fd, &buf[has_Written], len - has_Written);
    if (result < 0){
      printf("error in nwrite");
      return false;
    }else{
      //updates how much we written
      has_Written += result; 
    }
  }
  return true;
}

/* Through this function call the client attempts to receive a packet from sd 
(i.e., receiving a response from the server.). It happens after the client previously 
forwarded a jbod operation call via a request message to the server.  
It returns true on success and false on failure. 
The values of the parameters (including op, ret, block) will be returned to the caller of this function: 

op - the address to store the jbod "opcode"  
ret - the address to store the return value of the server side calling the corresponding jbod_operation function.
block - holds the received block content if existing (e.g., when the op command is JBOD_READ_BLOCK)

In your implementation, you can read the packet header first (i.e., read HEADER_LEN bytes first), 
and then use the length field in the header to determine whether it is needed to read 
a block of data from the server. You may use the above nread function here.  
*/
static bool recv_packet(int sd, uint32_t *op, uint16_t *ret, uint8_t *block) {
    //buffer with length HEADER_LEN
    uint16_t len;
    uint8_t header[HEADER_LEN];
  
    if (nread(sd, HEADER_LEN, header) == false) {
        return false;
    }
    //copy everything
    memcpy(&len, header, sizeof(len));
    memcpy(op, header + 2, sizeof(*op));
    memcpy(ret, header + 6, sizeof(*ret));
    //convert everything
    len = ntohs(len);
    *op = ntohl(*op);
    *ret = ntohs(*ret);
    //nread with all conditions check
    if (len == (HEADER_LEN + JBOD_BLOCK_SIZE)) {
        if (nread(sd, JBOD_BLOCK_SIZE, block) == false) {
            return false;
        }
    }else if (len == HEADER_LEN){
        return true;
    }else{
      return false;
    }
    return true;
}




/* The client attempts to send a jbod request packet to sd (i.e., the server socket here); 
returns true on success and false on failure. 

op - the opcode. 
block- when the command is JBOD_WRITE_BLOCK, the block will contain data to write to the server jbod system;
otherwise it is NULL.

The above information (when applicable) has to be wrapped into a jbod request packet (format specified in readme).
You may call the above nwrite function to do the actual sending.  
*/

//
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
    //buffer with length (header + block)
    uint8_t packet[HEADER_LEN + JBOD_BLOCK_SIZE];
    uint16_t len = HEADER_LEN;
    // get command from op code
    uint32_t cmd = op >> 14;
    
    if (cmd == JBOD_WRITE_BLOCK) {
        len += JBOD_BLOCK_SIZE;
    }
    //convert
    len = htons(len);
    op = htonl(op);
    //copy to packet with corresponding places
    memcpy(packet, &len, sizeof(len));
    memcpy(packet + 2, &op, sizeof(op));

   //when command is JBOD_WRITE_BLOCK, write block to packet
    if (cmd == JBOD_WRITE_BLOCK){
        //from block to buffer
        memcpy(&packet[HEADER_LEN], block, JBOD_BLOCK_SIZE);
        if(nwrite(sd, HEADER_LEN + JBOD_BLOCK_SIZE, packet) == false){
          return false;
        }
      }
    else{
      return nwrite(sd, HEADER_LEN, packet);
    }
    return true;
}




/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. 
 * this function will be invoked by tester to connect to the server at given ip and port.
 * you will not call it in mdadm.c
*/
bool jbod_connect(const char *ip, uint16_t port) {
  struct sockaddr_in caddr;
  int sock = socket(PF_INET, SOCK_STREAM, 0);
  //connect to the correct address
  caddr.sin_family = AF_INET;
  caddr.sin_port = htons(port);
  if(sock == -1){
    printf("sock fail");
    return false;
  }
  inet_aton(ip, &caddr.sin_addr);
  //condition for connection failed
  if (connect(sock, (const struct sockaddr *)&caddr, sizeof(caddr)) == -1){
    return false;
  }else{
    cli_sd = sock;
    return true;
  }
}




/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
  close(cli_sd);
  cli_sd = -1;
}



/* sends the JBOD operation to the server (use the send_packet function) and receives 
(use the recv_packet function) and processes the response. 

The meaning of each parameter is the same as in the original jbod_operation function. 
return: 0 means success, -1 means failure.
*/
int jbod_client_operation(uint32_t op, uint8_t *block) {
  //initialize 
  uint16_t ret;
  uint32_t new_op;
  if (send_packet(cli_sd, op, block) == false){
    return -1;
  } 
  //check all conditions, printf when there's an error(easier debug)
  if(recv_packet(cli_sd, &new_op, &ret, block) == true){
    if (new_op == op){
      if (ret == 0){
        return 0;
      }else{
        printf("ret was not 0");
      }
    }else{
      printf("pointer miss match");
    }
  }
  return -1;
}