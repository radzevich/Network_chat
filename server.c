/* A simple server in the internet domain using UDP
   The port number is passed as an argument
   This version runs forever, not forking off a separate
   process for each connection, but adding them to set
   and checking for ability to exchange data using select()
*/

/**************************TODO*************************
* Add definition of resending message length.
* Remove finishing program from logging.
* Change sender address to it's nickname.
* Something else but I don't remember.
*******************************************************/

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
#define NICKNAME_LENGTH 128
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


struct sfd_set
{
    fd_set system_set;
    unsigned short set[MAX_CONNECTION_NUM + 1];
    unsigned short count;
    unsigned short capacity;
};

typedef struct sfd_set sockfd_set;
typedef struct externalProocol externalProocol;


//Logging errors to ERROR_LOG.
void logError(const char *errorMessage);
//Logging progress to PROGRESS_LOG.
void logProgress(const char *progressMessage);
//Saving message history to MESSAGE_HISTORY.
void messageHistory(const char *heandle, int clientName, char* message);
//Closing program.
void finishProgram(int returnValue);
//Checking for unprocessed messages.
int waitToRead(sockfd_set *s_set);
//Getting messages from clients and writing them to pipe.
void readMessages(int fd, sockfd_set *s_set);
//Reading data from the pipe and resending it to clients.
void writeMessages(int fd, sockfd_set *s_set);
//Initializing sockfd structure.
void initializeSockfdSet(sockfd_set *s_set);
//Exchanging messages between reading and writing server processes
void chatting(sockfd_set *s_set);
//Conversion fields of external protocol structure to single text message for resending.
void externalProocolToChar(externalProocol *external, char* buf);
//Excluding sockfd from fd_set and internal array;
int excludeFromSockfdSet(unsigned short sockfd, sockfd_set *s_set);
//Adding sockfd to the set sructure.
void addToSockfdSet(unsigned short sockfd, sockfd_set *s_set);



//Main process. Creating socket and starting to listen for connections.
int main(int argc, char *argv[])
{
    int sockfd, portno, retCode, newsockfd, sock_shmid, fd_shmid;
    sockfd_set *s_set = NULL;
    socklen_t len;
    pid_t childpid;
    struct sockaddr_in addr;
    struct sockaddr cl_addr;


    if (argc < 2)
        logError("too few arguments\n");


    portno = atoi(argv[1]);
    //portno = atoi("57050");
    //shutdown(portno, SHUT_RDWR);
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

    sock_shmid = shmget(IPC_PRIVATE, sizeof(sockfd_set), IPC_CREAT | 0777);
    //fd_shmid = shmget(IPC_PRIVATE, sizeof(fd_set) * MAX_CONNECTION_NUM, IPC_CREAT | 0666);

    //Creating child process and sharing memory with parental one.
    if ((childpid = fork()) == 0)
    {
        if (NULL == (s_set = (sockfd_set *)shmat(sock_shmid, NULL, 0))) {
            logError("allocating sockfd_set");
            return 1;
        }

        chatting(s_set);
    }
    else
    {
        s_set = (sockfd_set *)shmat(sock_shmid, NULL, 0);
        //f_set = (fd_set *)shmat(fd_shmid, NULL, 0);
        initializeSockfdSet(s_set);
        printf("initialized\n");

        listen(sockfd, MAX_CONNECTION_NUM);

        while (!0)
        {
            len = sizeof(cl_addr);
            //newsockfd = accept(sockfd, &addr, &len);
            newsockfd = accept(sockfd, &cl_addr, &len);

            if (newsockfd < 0)
                logError("accepting connection");
            else
                logProgress("connection accepted..");

            addToSockfdSet(newsockfd, s_set);

            printf("socket added %d\n", newsockfd);
        }
    }

    return 0;
}


//Exchanging messages between reading and writing server processes
void chatting(sockfd_set *s_set)
{
    FD_ZERO(&s_set->system_set);
    int fd[2];
    pid_t reading_pid;

    pipe(fd);

    if((reading_pid = fork()) == -1)
    {
        logError("fork");
        exit(1);
    }
    else if (0 == reading_pid){
        logProgress("reading process created");
    }
    else {
        logProgress("writing process created");
    }


    if(reading_pid == 0)
    {
        //Child process closes input file descriptor.
        close(fd[0]);

        //Checking sockets state, getting messages from clients and sending them to resender through the pipe.
        readMessages(fd[1], s_set);
    }
    else
    {
        //Parent process closes output file descriptor.
        close(fd[1]);
        //Reading data from the pipe and resending them to clients.
        writeMessages(fd[0], s_set);
    }
}


//Checking for unprocessed messages.
int waitToRead(sockfd_set *s_set)
{
    int ready_sockfd_num = 0;
    struct timeval timeout;

    while (1)
    {
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;

        printf("waiting for message\n");
        printf("%d\n", s_set->count);

        ready_sockfd_num = select(s_set->count + 4, &s_set->system_set, NULL, NULL, &timeout);

        if (ready_sockfd_num == -1)
            logError("select failed");

        printf("Ready sockets: %d\n", ready_sockfd_num);
    
        return ready_sockfd_num;
    }
}


//Getting messages from clients and writing them to pipe.
void readMessages(int fd, sockfd_set *s_set)
{
    struct sockaddr_in clientAddr;
    //struct hostent *client;  //TODO
    struct internalProtocol local_mess;
    int ret_code = 0;
    size_t bytesToWrite;
    size_t totalWritten = 0;
    //char confirmed_buff[] = "confirmed";
    int len = sizeof(clientAddr);

    while (1)
    {
        if (waitToRead(s_set) == 0)
            continue;

        for (int i = 0; i < s_set->count; i++)
        {
            if (FD_ISSET(s_set->set[i], &s_set->system_set))
            {
                ret_code = recvfrom(s_set->set[i], local_mess.external.text, BUFF_SIZE, 0, (struct sockaddr *) &clientAddr, (socklen_t *)&len);
                //client = gethostbyaddr((char*)&clientAddr.sin_addr.s_addr, sizeof(struct in_addr), AF_INET);

                if(ret_code < 0)
                    logError("receiving data");
                else
                    logProgress("receiving data..");

                local_mess.external.sender_addr = clientAddr.sin_addr.s_addr;
                local_mess.sockfd = s_set->set[i];

                //Writing text of message and sender's address to pipe.
                bytesToWrite = sizeof(struct internalProtocol);
                while (1)
                {
                    ssize_t bytesWritten = write(fd, &local_mess + totalWritten, bytesToWrite - totalWritten);

                    if ( bytesWritten <= 0 )
                        break;

                    totalWritten += bytesWritten;
                }
            }
        }
    }
}


//Reading data from the pipe and resending it to clients.
void writeMessages(int fd, sockfd_set *s_set)
{
    struct sockaddr_in clientAddr;
    //struct hostent *client;  //TODO
    struct internalProtocol local_mess;
    size_t bytesToWrite;
    size_t totalWritten = 0;
    char confirmed_buff[] = "confirmed";
    char resending_mess[BUFF_SIZE + NICKNAME_LENGTH];
    int len = 0, ret_code = 0;

    //Reading text of message and sender's address from the pipe.
    bytesToWrite = sizeof(local_mess);

    while (1)
    {
        ssize_t bytesWritten = read(fd, &local_mess + totalWritten, bytesToWrite - totalWritten);

        if ( bytesWritten <= 0 )
            break;

        totalWritten += bytesWritten;
    }

    len = sizeof(clientAddr);
    externalProocolToChar(&local_mess.external, resending_mess);

    //Resending messages from server to clients.
    for (int i = 0; i < s_set->count; i++)
    {
        if (s_set->set[i] != local_mess.sockfd)
            ret_code = sendto(s_set->set[i], resending_mess, NICKNAME_LENGTH + BUFF_SIZE, 0, (struct sockaddr *)&clientAddr, len);
        else
            //If current client is sender, server confirmes getting message instead of resending his message back.
            ret_code = sendto(s_set->set[i], confirmed_buff, strlen(confirmed_buff), 0, (struct sockaddr *)&clientAddr, len);

        if (ret_code < 0)
        {
            close(s_set->set[i]);
            excludeFromSockfdSet(s_set->set[i], s_set);
        }
        else                                                                 //TODO remove log
            logProgress("sending data..");
    }
}


//TODO remove program finishing from loging
void logError(const char *errorMessage)
{
    fprintf(ERROR_LOG, "ERROR: %s\n", errorMessage);
    finishProgram(1);
}

void logProgress(const char *progressMessage)
{
    fprintf(PROGRESS_LOG, "log: %s\n", progressMessage);
}

void messageHistory(const char *heandle, int clientName, char* message)
{
    fprintf(MESSAGE_HISTORY, "%s %d: %s\n", heandle, clientName, message);
}

void finishProgram(int returnValue)
{
    exit(returnValue);
}


sockfd_set *createSockfdSet()
{
    sockfd_set *s_set = NULL;

    s_set = (sockfd_set *)calloc(1, sizeof(sockfd_set));

    if (NULL != s_set) {
        //s_set->set = (unsigned short *)calloc(MAX_CONNECTION_NUM + 1, sizeof(unsigned short));
        //s_set->system_set = (fd_set *)calloc(1, sizeof(fd_set));

        if ((NULL == s_set->set) || (NULL == &s_set->system_set)) {
            free(s_set);
            logError("sockfd_set of fd_set allocating");
            return NULL;
        }
            s_set->count = 0;
            s_set->capacity = MAX_CONNECTION_NUM;
    }
    else {
        logError("sockfd_set allocating");
        return NULL;
    }

    return s_set;
}

//Conversion fields of external protocol structure to single text message for resending.
void externalProocolToChar(externalProocol *external, char* buf)
{
    int len = 0;
/*
    strlen(external->sender_addr);
    memset(buf, 0, NICKNAME_LENGTH + BUFF_SIZE);

    if (len > NICKNAME_LENGTH) {
        len = NICKNAME_LENGTH;
    }

    for (int i = 0; i < len; i++) {
        buf[i] = external->sender_addr[i];
    */
    len = strlen(external->text);

    for (int i = NICKNAME_LENGTH; i < len; i++) {
        buf[i] = external->text[i];
    }
}

//Initializing sockfd structure.
void initializeSockfdSet(sockfd_set *s_set)
{
    s_set->count = 11;
    s_set->capacity = MAX_CONNECTION_NUM;
    FD_ZERO(&s_set->system_set);
    memset(s_set->set, 0, sizeof(s_set) * (s_set->capacity + 1));
}


void addToSockfdSet(unsigned short sockfd, sockfd_set *s_set)
{
    s_set->set[s_set->count] = sockfd;
    FD_SET(sockfd, (&s_set->system_set));
    s_set->count++;
}

//Excluding sockfd from fd_set and internal array;
int excludeFromSockfdSet(unsigned short sockfd, sockfd_set *s_set)
{
    int index = 0;

    while ((index < s_set->count) && (s_set->set[index] != sockfd)) {
        index++;
    }

    if (s_set->set[index] != sockfd) {
        logError("sockfd not in the set");
        return 1;
    }

    for (int i = index; i < s_set->count; i++) {
        s_set->set[index] = s_set->set[index + 1];
    }

    FD_CLR(sockfd, &s_set->system_set);
    s_set->count--;

    return 0;
}

void clearSockfdSet(sockfd_set *s_set)
{
    free(s_set->set);
    free(s_set);
}

