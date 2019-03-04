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

#define PORT    44460
#define MAXLINE 1420

#define ANSI_COLOR_RED  "\x1b[31m"
#define ANSI_COLOR_RST  "\x1b[0m"

int parse_packet(char *buf){

  char *token;
  int cnt = 0;
  int res = -1;

  while ((token = strsep(&buf, "_")) != NULL){
    if (cnt==2){
      //printf("%s\n", token);
      res = atoi(token);
      break;
    }
    cnt++;
  }

  return res;
}

// server
int main() {

    int fd;
    char buffer[MAXLINE];
    struct sockaddr_in servaddr, cliaddr;

    if ( (fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr,  0, sizeof(cliaddr));

    // Server info
    servaddr.sin_family      = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port        = htons(PORT);

    // Bind the socket with the server address
    if ( bind(fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 )
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    int cliaddr_len, n;
    bool first = true;
    int pnum;
    int pnum_old=-1;

    printf("Listening at %s:%d\n",inet_ntoa(servaddr.sin_addr),PORT);

    // Receive
    for(;;){

      n = recvfrom(fd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &cliaddr, (socklen_t *) &cliaddr_len);
      buffer[n] = '\0';

      if (first&&(cliaddr.sin_port!=0)){
        printf("Receiving from %s:%d\n", inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port));
        first = false;
      }

      pnum = parse_packet(buffer);
      printf("packet %d\n",pnum);

      if (pnum_old!=-1){
        if ((pnum-pnum_old)!=1){
          printf(ANSI_COLOR_RED "PACKET LOSS, received:  %d, expected:  %d" ANSI_COLOR_RST "\n", pnum, pnum_old+1);
        }
      }

      pnum_old = pnum;

    }

    return 0;
}
