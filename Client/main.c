#include"stdio.h"
#include"stdlib.h"
#include"sys/types.h"
#include"sys/socket.h"
#include"string.h"
#include"netinet/in.h"
#include"netdb.h"

#define BUFF_SIZE 1000

int main(int argc, char *argv[])
{
    struct sockaddr_in addr, cl_addr;
    int sockfd, retCode;
    char buffer[BUFF_SIZE];
    struct hostent * server;
    char *serverAddr;

    if (argc < 3) {
        printf("usage: client < ip address >\n");
        exit(1);
    }

    serverAddr = argv[1];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        printf("Error creating socket!\n");
        exit(1);
    }
    else
        printf("Socket created...\n");

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(serverAddr);
    addr.sin_port = PORT;

    retCode = connect(sockfd, (struct sockaddr *) &addr, sizeof(addr));

    if (retCode < 0) {
        printf("Error connecting to the server!\n");
        exit(1);
    }
    else
        printf("Connected to the server...\n");

    memset(buffer, 0, BUFF_SIZE);
    printf("Enter your message(s): ");

    while (fgets(buffer, BUFF_SIZE, stdin) != NULL) {
        retCode = sendto(sockfd, buffer, BUFF_SIZE, 0, (struct sockaddr *) &addr, sizeof(addr));

        if (retCode < 0) {
            printf("Error sending data!\n\t-%s", buffer);
        }

        retCode = recvfrom(sockfd, buffer, BUFF_SIZE, 0, NULL, NULL);

        if (retCode < 0) {
            printf("Error receiving data!\n");
        }
        else {
            printf("Received: ");
            fputs(buffer, stdout);
            printf("\n");
        }
 }

 return 0;
}
