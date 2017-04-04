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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define CL_HOST_NAME_LEN 100
#define MAX_CONNECTION_NUM 5
#define BUFF_SIZE 1024
#define PROGRESS_LOG stdout
#define ERROR_LOG stderr
#define MESSAGE_HISTORY stdout


//Structure for exchanging data between server and clients.
struct externalProocol 
{
    unsigned long sender_addr;
    char text[BUFF_SIZE];
};

//Structure for exchanging data between server's reading and writing processes.
struct internalProtocol
{
    int sockfd;
    struct externalProocol external; 
};


//Logging errors to ERROR_LOG.
void logError(const char *errorMessage);
//Logging progress to PROGRESS_LOG.
void logProgress(const char *progressMessage);
//Saving message history to MESSAGE_HISTORY.
void messageHistory(const char *heandle, int clientName, char* message);
//Closing program.
void finishProgram(int returnValue);


//Main process. Creating socket and starting to listen for connections.
int main(int argc, char *argv[])
{
    int sockfd, portno, retCode, newsockfd, shmid;
    socklen_t len;
    fd_set *sockfd_set = NULL;
    pid_t childpid;
    struct sockaddr_in addr;
    char buff[BUFF_SIZE];


    if (argc < 2)
        logError("too few arguments\n");

    portno = atoi(argv[1]);
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

    shmid = shmhet(IPC_PRIVATE, sizeof(fd_set) * MAX_CONNECTION_NUM, IPC_CREATE | 0666);

    if ((childpid = fork()) == 0)
    {
        sockfd_set = shmat(shmid, NULL, 0);
    }
    else
    {
        sockfd_set = shmat(shmid, NULL, 0);
    }

    listen(sockfd, MAX_CONNECTION_NUM);
    FD_ZERO(sockfd_set);

    while (!0)
    {
        newsockfd = accept(sockfd, NULL, NULL);

        if (newsockfd < 0)
            logError("accepting connection");
        else
            logProgress("Connection accepted..");

        FD_SET(newsockfd, sockfd_set);
    }

    return 0;
}


void chatting(fd_set *sockfd_set)
{
    FD_ZERO(sockfd_set);
    int fd[2], nbytes;
    pid_t reading_pid;
    pipe(fd);

    if((reading_pid = fork()) == -1)
    {
        logError("fork");
        exit(1);
    }

    while(!0)
    {
        if(reading_pid == 0)
        {
            //Child process closes input file descriptor.
            close(fd[0]);

            //Checking sockets state, getting messages from clients and resending them to pipe.
            if (waitToRead(sockfd_set) != 0)
                readMessages(fd[1], sockfd_set);
        }
        else
        {
            //Parent process closes output file descriptor.
            close(fd[1]);
            //Reading data from the pipe and resending them to clients.
            writeMessages(fd, sockfd_set);
        }
    }
}

//Checking for unprocessed messages. 
int waitToRead(fd_set *sockfd_set)
{
    int ready_sockfd_num = 0;
    struct timeval timeout;

    //Set time limit.
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    
    ready_sockfd_num = select(sockfd_set->fd_count, &sockfd_set, NULL, NULL, &timeout);

    if (ready_sockfd_num == -1) 
        logError("select failed");

    return ready_sockfd_num;
}

//Getting messages from clients and writing them to pipe. 
void readMessages(int fd, fd_set *sockfd_set)
{
    struct sockaddr_in clientAddr;
    //struct hostent *client;  //TODO
    struct internalProtocol local_mess;
    int ret_code = 0;
    size_t bytesToWrite;
    size_t totalWritten = 0;
    char confirmed_buff = "confirmed";
    int len = sizeof(clientAddr);

    for (int i = 0; i < sockfd_set->fd_count, i++)
    {
        if (FD_ISSET(sockfd_set->fd_array[i]))
        {
            ret_code = recvfrom(sockfd_set->fd_array[i], local_mess.external.buff, BUFF_SIZE, 0, (struct sockaddr *) &clientAddr, &len);
            //client = gethostbyaddr((char*)&clientAddr.sin_addr.s_addr, sizeof(struct in_addr), AF_INET);

            if(ret_code < 0)
                logError("receiving data");
            else
                logProgress("receiving data.."); 

            local_mess.external.sender_addr = clientAddr.sin_addr.s_addr;
            local_mess.sockfd = sockfd_set->fd_array[i]);

            //Writing text of message and sender's address to pipe.
            bytesToWrite = sizeof(struct internalProtocol);
            while (1)
            {
                ssize_t bytesWritten = write(fd, &_local_mess + totalWritten, bytesToWrite - totalWritten);
                
                if ( bytesWritten <= 0 )
                    break;

                totalWritten += bytesWritten;
            }
        }
    }   
}

//Reading data from the pipe and resending it to clients.
void writeMessages(int fd, fd_set *sockfd_set)
{
    struct sockaddr_in clientAddr;
    //struct hostent *client;  //TODO
    struct internalProtocol local_mess;
    int ret_code = 0;
    size_t bytesToWrite;
    size_t totalWritten = 0;
    char confirmed_buff = "confirmed";
    int len = sizeof(clientAddr);

    //Reading text of message and sender's address from the pipe.
    bytesToWrite = sizeof(local_mess);

    while (1)
    {
        ssize_t bytesWritten = read(fd, &local_mess + totalWritten, bytesToWrite - totalWritten);
                
        if ( bytesWritten <= 0 )
            break;

        totalWritten += bytesWritten;
    }

    //Resending messages from server to clients.
    for (int i = 0; i < sockfd_set->fd_count, i++)
    {
        if (sockfd_set->fd_array[i] != local_mess.sockfd)
            retCode = sendto(sockfd_set->fd_array[i], local_mess.external, sizeof(struct externalProocol), 0, (struct sockaddr *)&clientAddr, len);
        else
            //If current client is sender, server confirmes getting message instead of resending his message back.
            retCode = sendto(sockfd_set->fd_array[i], confirmed_buff, strlen(confirmed_buff), 0, (struct sockaddr *)&clientAddr, len);

        if (retCode < 0)
        {
            close(sockfd_set->fd_array[i]);
            FD_CLR(sockfd_set->fd_array[i], sockfd_set);
        }
        else                                                                 //TODO remove log
            logProgress("sending data..");        
    }   
}


void logError(const char *errorMessage)
{
    fprintf(ERROR_LOG, "ERROR: %s\n", errorMessage);
    finishProgram(1);
}

void logProgress(const char *progressMessage)
{
    //fprintf(PROGRESS_LOG, "log: %s\n", progressMessage);
}

void messageHistory(const char *heandle, int clientName, char* message)
{
    fprintf(MESSAGE_HISTORY, "%s %d: %s\n", heandle, clientName, message);
}

void finishProgram(int returnValue)
{
    exit(returnValue);
}