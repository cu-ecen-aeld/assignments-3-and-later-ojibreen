
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define PORT "9000"
#define BACKLOG 100
#define FILENAME "/var/tmp/aesdsocketdata"
#define DEVICEPATH "/dev/aesdchar"
#define BUFFERSIZE 1024
#define TIMESTAMPDELAY 10

static struct sigaction sig_action = {0};
pthread_mutex_t fileMutex;
bool terminate = false;

void signalHandler(int signo) {
    if(signo == SIGINT || signo == SIGTERM) {
        syslog(LOG_USER, "Caught termination signal. Exiting.");
        terminate = true;
    }
}

int initSignalHandler(void) {
    sig_action.sa_handler = signalHandler;
    memset(&sig_action.sa_mask, 1, sizeof(sig_action.sa_mask));
    if(sigaction(SIGINT, &sig_action, NULL) != 0) {
        perror("sigaction");
        syslog(LOG_ERR, "sigaction failed for SIGINT.");
        return -1;
    }

    if(sigaction(SIGTERM, &sig_action, NULL) != 0) {
        perror("sigaction");
        syslog(LOG_ERR, "sigaction failed for SIGTERM.");
        return -1;
    }
    syslog(LOG_USER, "Signal handler setup succeeded.");
    return 0;
}

// Writes timestamps to the data file on an interval.
void timestampHandler(void) {
    while(1) {
        pthread_mutex_lock(&fileMutex);
        if(terminate) {
            pthread_mutex_unlock(&fileMutex);
            return;
        }
        FILE *dataFile = fopen(DEVICEPATH, "w");
        if(dataFile == NULL) {
            perror("fopen");
            syslog(LOG_ERR, "Failed to open data file.");
            pthread_mutex_unlock(&fileMutex);
            sleep(TIMESTAMPDELAY);
            continue;
        }

        time_t now = time(NULL);
        struct tm *pTimeData = localtime(&now);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "timestamp:%Y-%m-%d %H:%M:%S\n", pTimeData);

        fprintf(dataFile, "%s", timestamp);
        fclose(dataFile);
        pthread_mutex_unlock(&fileMutex);

        sleep(TIMESTAMPDELAY);
    }
    return;
}

// Starts a thread to handle timestamp write operations to file.
int initTimestampHandler(void) {
    pthread_t timestampThread;
    if(pthread_create(&timestampThread, NULL, (void *) timestampHandler, NULL) < 0) {
        perror("Timestamp thread failed");
        return-1;
    }
    return 0;
}

// Handles data exchange after a socket connetion is made.
void *exchangeData(void * pConnFd) {
    int connectionFd = *((int *) pConnFd);
    pthread_mutex_lock(&fileMutex);
    FILE *dataFile = fopen(DEVICEPATH, "w");
    if(!dataFile){
        syslog(LOG_ERR, "Unable to open file for writing.");
        pthread_mutex_unlock(&fileMutex);
        return 0;
    }
    
    size_t bytesReceived = 0;
    char buffer[BUFFERSIZE];

    while((bytesReceived = recv(connectionFd, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytesReceived, dataFile);
        if(memchr(buffer, '\n', bytesReceived) != NULL) {
            break;
        }
    }
    fclose(dataFile);
    
    // Send back the contents of the file to the client.
    ssize_t bytesRead = 0;
    dataFile = fopen(DEVICEPATH, "r");
    if(!dataFile) {
        syslog(LOG_ERR, "Error opening data file.");
        return 0;
    }

    // Reset buffer and start sending.
    memset(buffer, 0, sizeof(buffer));
    while((bytesRead = fread(buffer, 1, sizeof(buffer), dataFile))){
        if((send(connectionFd, buffer, bytesRead, 0)) != bytesRead) {
            syslog(LOG_ERR, "Error sending file contents over socket connection.");
            return 0;
        }
    }
    fclose(dataFile);
    pthread_mutex_unlock(&fileMutex);
    return 0;
}

// Initiates socket connections and invokes data exchange operations.
int initConnectionHandler(int *pSocketFd, struct sockaddr_storage *pSockAddrStorage) {
    socklen_t sockAddrStorageSize = sizeof(*pSockAddrStorage);
    while(1) {
        if(terminate) {
            // A termination signal was sent. Immediately break out of this loop and exit the function.
            return 0;
        }
        
        pthread_t threadId;
        int connectionFd = accept(*pSocketFd, (struct sockaddr *) pSockAddrStorage, &sockAddrStorageSize);
        if (connectionFd == -1){
            syslog(LOG_ERR, "Eroor accepting connection.");
            continue;
        } else {
            syslog(LOG_USER, "Accepted connection.");
            
            if(pthread_create(&threadId, NULL, exchangeData, (void *) &connectionFd) < 0) {
                perror("pthread_create");
                syslog(LOG_ERR, "Unable to create thread while accepting connection.");
                return -1;
            }
            pthread_join(threadId , NULL);
        }
    }
    return 0;
}

int daemonize(int *pSocketFd, struct sockaddr_storage *pSockAddrStorage) {
    pid_t pid = fork();
    if(pid == -1) {
        syslog(LOG_ERR, "Unable to fork.");
        perror("fork");
        return -1;
    } else if(pid != 0) {
        syslog(LOG_USER, "Running in daemon mode.");
        exit(EXIT_SUCCESS);
    }
        
    // Create new session and process group.
    if(setsid() == -1) {
        syslog(LOG_ERR, "setsid failed after fark.");
        perror("setsid");
        return -1;
    }
    
    // CHILD PROCESS STARTED
    // Set the working directory of child processto the root directory.
    if(chdir("/") == -1) {
        syslog(LOG_ERR, "chdir failed after fork.");
        perror("chdir");
        return -1;
    }

    // Redirect fds to /dev/null
    int nullFd = open("/dev/null", O_WRONLY | O_CREAT, 0666);
    dup2(nullFd, 1);

    // Start the timestamp thread.
    // initTimestampHandler();
    
    // Begin accepting connections and exchanging data.
    initConnectionHandler(pSocketFd, pSockAddrStorage);
    
    // After the program is done accepting connections (i.e. after a term signal is received, start cleaning up.
    syslog(LOG_USER, "Closing connections and cleaning up.");
    remove(FILENAME);
    
    if(nullFd != -1) {
        close(nullFd);
    }

    if(*pSocketFd != -1) {
        close(*pSocketFd);
    }
    
    closelog();
    exit(EXIT_SUCCESS);
}

// Main routine.
int main(int argc, char *argv[]) {
    openlog(NULL, 0, LOG_USER);
    struct sockaddr_storage sockAddrStorage;
    struct addrinfo hints, *pAddrInfo;
    int socketFd;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    bool runAsDaemon = false;
    
    for(int i = 1; i < argc; i++) {
        if (strstr(argv[i], "-d")) {
            runAsDaemon = true;
            break;
        }
    }

    // Initialize the signal handler.
    if(initSignalHandler() == -1) {
        return -1;
    }

    // Used to get network and host information for socket binding and listening.
    getaddrinfo(NULL, PORT, &hints, &pAddrInfo);

    socketFd = socket(pAddrInfo->ai_family, pAddrInfo->ai_socktype, pAddrInfo->ai_protocol);
    if (socketFd == -1){
        freeaddrinfo(pAddrInfo);
        syslog(LOG_ERR, "Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to address and port.
    if(bind(socketFd, pAddrInfo->ai_addr, pAddrInfo->ai_addrlen) == -1) {
        syslog(LOG_ERR, "Could not bind socket to address.");
        freeaddrinfo(pAddrInfo);
        exit(EXIT_FAILURE);
    }
    
    // Free dynamically allocated memory.
    freeaddrinfo(pAddrInfo);

    // Start listening for connections.
    if(listen(socketFd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Could not invoke listen on socket.");
    }
    
    // Check if we're running as a daemon. If so, fork a new process.
    if(runAsDaemon) {
        daemonize(&socketFd, &sockAddrStorage);
    }

    // PARENT PROCESS
    // Running in non-daemon mode.
    syslog(LOG_USER, "Running in interactive mode.");

    // Start the timestamp thread.
    // initTimestampHandler();
    
    // Begin accepting connections and exchanging data.
    initConnectionHandler(&socketFd, &sockAddrStorage);
    
    // After the program is done accepting connections (i.e. after a term signal is received, start cleaning up.
    syslog(LOG_USER, "Closing connections and cleaning up.");
    remove(FILENAME);
    
    if(socketFd != -1) {
        close(socketFd);
    }
    closelog();
    exit(EXIT_SUCCESS);
}



