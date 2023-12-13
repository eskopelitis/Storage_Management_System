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

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
bool nread(int fd, int len, uint8_t *buf) {
  int total = 0;
  int x;
  while(total<len){
    x = read(fd,buf+total,len-total);
    //reattempt
    if(x<=0){
      if(x==-1 && errno == EINTR){continue;}
      return false;
    }
    total = total + x;
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
bool nwrite(int fd, int len, uint8_t *buf) {
  int total = 0;
  int x;
  while(total < len){
    x = write(fd,buf+total,len-total);
    //reattempt
    if(x<=0){
      if(x==-1 && errno == EINTR){continue;}
      return false;
    }
    total=total+x;
  }
  return true;
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
bool recv_packet(int fd, uint32_t *op, uint8_t *ret, uint8_t *block) {
  uint8_t l_head[HEADER_LEN];
  uint16_t length;
  nread(fd, HEADER_LEN, l_head);
  //get length, op, and status
  memcpy(&length, l_head, 2);
  memcpy(op, l_head+2, 4);
  memcpy(ret, l_head+6, 2);

  *ret = ntohs(length);
  length = ntohs(length);
  *op = ntohl(*op);
  //if length passes, reads data from block, else false
  if(length == 264){nread(fd, 256, block);}
  else{return false;}
  return true;
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
bool send_packet(int sd, uint32_t op, uint8_t *block) {
  uint32_t comm = op>>26;
  uint16_t length = HEADER_LEN;
  op = htonl(op);
  // do commands if isn't write block
  if(comm != JBOD_WRITE_BLOCK){
    uint8_t pack[HEADER_LEN];
    length = htons(length);
    //copy and send to packet
    memcpy(pack+2, &op, 4);
    memcpy(pack,&length,2);
    nwrite(sd, 8, pack);
  }
  //commands if it's write block
  else{
    uint8_t pack[HEADER_LEN+JBOD_BLOCK_SIZE];
    length = length+JBOD_BLOCK_SIZE;
    length = htons(length);//convert

    memcpy(pack+2, &op, 4);
    memcpy(pack, &length, 2);
    memcpy(pack+8, block, 256);
    nwrite(sd, 264,pack);
  }
  return true;
}

/* connect to server and set the global client variable to the socket */
bool jbod_connect(const char *ip, uint16_t port) {
  struct sockaddr_in connect_server;
  connect_server.sin_family = AF_INET;
  connect_server.sin_port = htons(JBOD_PORT);

  if(inet_aton(ip, &(connect_server.sin_addr)) == 0){return false;}
  //new socket
  cli_sd= socket(PF_INET, SOCK_STREAM,0);

  //returns -1 if can't create socket
  if(cli_sd==-1){return false;}
  if(connect(cli_sd, (struct sockaddr *)&connect_server, sizeof(connect_server))==-1){return false;}//return false if connection fails
  return true;
}

void jbod_disconnect(void) {
  //disconnects
  if(cli_sd!=-1){close(cli_sd);}
  cli_sd = -1;
}

int jbod_client_operation(uint32_t op, uint8_t *block) {
  uint32_t cop;
  uint8_t retrieve_R;
  //tries to send packet
  if(send_packet(cli_sd,op,block)){recv_packet(cli_sd,&cop, &retrieve_R, block);}
  return retrieve_R; // gives status return
}
