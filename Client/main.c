#include "stdio.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>


#define BUFF_SIZE 1024

int main(int argc, char *argv[])
{
    struct sockaddr_in addr;
    int sockfd, retCode, pid;
    char buffer[BUFF_SIZE];
    struct hostent *server;

    if (argc < 3) {
        printf("usage: client < ip address >\n");
        exit(1);
    }

    server = gethostbyname(argv[1]);
    //server = gethostbyname("127.0.0.1");
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        printf("Error creating socket!\n");
        exit(1);
    }
    else
        printf("Socket created...\n");

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    bcopy((char*)server->h_addr_list[0], (char*)&addr.sin_addr.s_addr, server->h_length);
    addr.sin_port = htons((short)atoi(argv[2]));
    //addr.sin_port = htons(57057);

    retCode = connect(sockfd, (struct sockaddr *) &addr, sizeof(addr));

    if (retCode < 0) {
        printf("Error connecting to the server!\n");
        exit(1);
    }
    else
        printf("Connected to the server...\n");

    if ((pid = fork()) < 0) {
        printf("Error creating process: %s\n", strerror(errno));
    }
    else if (pid > 0)
    {
        while (1)
        {
            memset(buffer, 0, BUFF_SIZE);
            printf("Enter your message(s): ");
            fgets(buffer, BUFF_SIZE, stdin);
            retCode = sendto(sockfd, buffer, BUFF_SIZE, MSG_CONFIRM, (struct sockaddr *) &addr, sizeof(addr));
            printf("RET_w: %d\n", retCode);
            sleep(2);

            if (retCode <= 0) {
                printf("Error sending data!\n\t-%s", buffer);
                break;
            }
            else
                printf("sended\n");
        }
    }
    else
    {
        while (1)
        {
            memset(buffer, 0, BUFF_SIZE);
                  
            retCode = recvfrom(sockfd, buffer, BUFF_SIZE, 0, NULL, NULL);
            printf("RET_r: %d\n", retCode);
            sleep(2);   
            if (retCode <= 0) {
                printf("Error receiving data!\n");
                break;
            }
            else {
                printf("\nReceived: %s\n", buffer);
            } 
        }  
    }
    return 0;
}
