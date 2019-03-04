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
#include <sys/uio.h>

#define PORT    44460
#define MAXLINE 1420-67

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

	struct sockaddr_in saddr;
	memset(&saddr, 0, sizeof(struct sockaddr_in));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(0);
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(fd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in))<0){
		printf("BIND ERROR!");
	}

    printf("Sending to %s:%d\n",inet_ntoa(servaddr.sin_addr),PORT);

    int frame_cnt = 0;
    int pnum = 0;

    // connect to server?
    if (connect(fd,(const struct sockaddr *) &servaddr, sizeof(servaddr))<0){
    	printf("connect() error: %d",errno);
    	return -2;
    }

    // Send
    while(true){
      for(int i=0;i<400;i++){
        generate_packet(buffer,pnum);
        // tested
        //res = sendto(fd, (const char *)buffer, strlen(buffer), 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
        // tested
        //res = write(fd,(const char *)buffer,strlen(buffer));

        int iovcnt;
        struct iovec iov[3];

        char *buf1 = "This is a longer string\n";
        char *buf2 = "This is the longest string in this example\n";
        iov[0].iov_base = buffer;
        iov[0].iov_len = strlen(buffer);
        iov[1].iov_base = buf1;
        iov[1].iov_len = strlen(buf1);
        iov[2].iov_base = buf2;
        iov[2].iov_len = strlen(buf2);

        iovcnt = 3;

        res = writev(fd,iov,iovcnt);

        if (res<0){
          printf("    send result= %d,  errno= %d\n",res,errno);
        }else{
          printf("    sent %d bytes\n",res);
        }
        pnum++;
      }
      //printf("%s\n",buffer);
      printf("Sent frame %d, total packets %d\n",frame_cnt, pnum);
      usleep(100000);
      frame_cnt++;
    }

    return 0;
}
