/*
 * Based on:
 *   https://www.geeksforgeeks.org/udp-server-client-implementation-c/
 */

// Server side implementation of UDP client-server model
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

#define PORT    44460
#define MAXLINE 1420

#define ANSI_COLOR_RED  "\x1b[31m"
#define ANSI_COLOR_RST  "\x1b[0m"

int random_string(char *dest, size_t length) {

  char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';

    return 0;
}

int generate_packet(char * buf, int pnum){

    char pnum_str[15];
    sprintf(pnum_str,"__%d__",pnum);
    random_string(buf,MAXLINE);
    strncpy(buf,pnum_str,strlen(pnum_str));

    return 0;
}

//client
int main(int argc, char **argv) {

    int fd;
    char buffer[MAXLINE];
    int res = 0;

    struct sockaddr_in servaddr;
    in_addr_t s_addr = INADDR_ANY;

    if (argc>1){
      if (!inet_aton(argv[1],&s_addr)){
        perror("Invalid inet addr in argv[1]");
        return -1;
      }
    }

    if ( (fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Server info
    servaddr.sin_family      = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = s_addr;
    servaddr.sin_port        = htons(PORT);

    printf("Sending to %s:%d\n",inet_ntoa(servaddr.sin_addr),PORT);

    int frame_cnt = 0;
    int pnum = 0;

    // Send
    for(;;){
      for(int i=0;i<200;i++){
        generate_packet(buffer,pnum);
        res = sendto(fd, (const char *)buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
        if (res<0){
          printf("    send result= %d,  errno= %d\n",res,errno);
        }else{
          printf("    sent %d bytes\n",res);
        }
        pnum++;
      }
      //printf("%s\n",buffer);
      printf("Sent frame %d\n",frame_cnt);
      sleep(1);
      frame_cnt++;
    }

    return 0;
}
