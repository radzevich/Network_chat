/* A simple server in the internet domain using TCP
   The port number is passed as an argument
   This version runs forever, forking off a separate
   process for each connection
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFF_SIZE 1024
#define CL_HOST_NAME_LEN 100
#define LISTEN_QUEUE_LEN 5
#define PROGRESS_LOG stdout
#define ERROR_LOG stderr
#define MESSAGE_HISTORY stdout

//Logging errors to ERROR_LOG.
void logError(const char *errorMessage);
//Logging progress to PROGRESS_LOG.
void logProgress(const char *progressMessage);
//Saving message history to MESSAGE_HISTORY.
void messageHistory(const char *heandle, char* clientName, char* message);
//Closing program.
void finishProgram(int returnValue);

//Main process. Creating socket and starting to listen for connections.
int main(int argc, char *argv[])
{
    int sockfd, portno, retCode, newsockfd, childpid;
    socklen_t len;
    struct sockaddr_in addr, clientAddr;
    char buff[BUFF_SIZE], clientHostName[CL_HOST_NAME_LEN];

    if (argc < 3)
        logError("too few arguments\n");

    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
        logError("socket creating error");
    else
        logProgress("socket created..");

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((short)portno);
    addr.sin_addr.s_addr = INADDR_ANY;

    retCode = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));

    if (retCode < 0)
        logError("binding error");
    else
        logProgress("binding done..");

    logProgress("waiting for connections..");
    listen(sockfd, LISTEN_QUEUE_LEN);

    while (true)
    {
        len = sizeof(clientAddr);
        newsockfd = accept(sockfd, (struct sockaddr *) &clientAddr, &len);

        if (newsockfd < 0)
            logError("accepting connection");
        else
            logProgress("Connection accepted..");

        //inet_ntop(AF_INET, &(clientAddr.sin_addr), clientAddr, CL_HOST_NAME_LEN);

        //creating a child process
        if ((childpid = fork()) == 0)
        {
            //stop listening for new connections by the main process.
            //the child will continue to listen.
            //the main process now handles the connected client.
            close(sockfd);

            while (true)
            {
                memset(buff, 0, BUFF_SIZE);
                retCode = recvfrom(newsockfd, buff, BUFF_SIZE, 0, (struct sockaddr *) &clientAddr, &len);

                if(retCode < 0)
                    logError("receiving data");
                else
                    logProgress("receiving data..");

                messageHistory("Received data from", clientHostName, buff);
                retCode = sendto(newsockfd, buff, BUFF_SIZE, 0, (struct sockaddr *) &clientAddr, len);

                if (retCode < 0)
                    logError("sending data");
                else
                    logProgress("sending data..");

                messageHistory("Sent data to", clientHostName, buff);
            }
        }
        close(newsockfd);
    }
}

void logError(const char *errorMessage)
{
    fprintf(ERROR_LOG, "ERROR: %s\n", errorMessage);
    finishProgram(1);
}

void logProgress(const char *progressMessage)
{
    fprintf(PROGRESS_LOG, "PROGRESS: %s\n", progressMessage);
}

void messageHistory(const char *heandle, char* clientName, char* message)
{
    fprintf(MESSAGE_HISTORY, "%s %s: %s\n", heandle, clientName, message);
}

void finishProgram(int returnValue)
{
    exit(returnValue);
}
