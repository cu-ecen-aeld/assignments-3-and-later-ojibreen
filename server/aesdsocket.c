#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
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
#include <unistd.h>

#define PORT "9000"
#define BACKLOG 10
#define FILENAME "/tmp/aesdsocketdata"
#define BUFFERSIZE 1024

// Used to store pointers to file descriptors, structs and variables needed for socket management.
typedef struct {
    struct sockaddr_storage *pSockAddrStorage;
    socklen_t *pSockAddrStorageSize;
    struct addrinfo *pHints;
    struct addrinfo *pAddrInfo;
    int *pSocketFd;
    int *pAcceptConnectionFd;
} socket_context;

static struct sigaction sig_action = {0};


// Flag set by SIGTERM and SIGINT to notify functions to stop what they're doing and exit.
bool terminate = false;

void signalHandler(int signo) {
    if(signo == SIGINT || signo == SIGTERM) {
        syslog(LOG_USER, "Caught termination signal. Exiting.");
        terminate = true;
    }
}

int initSignalHandler() {
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

int sendFileContentsOverSocket(socket_context *socketContext) {
	ssize_t bytesRead = 0;
	FILE *dataFile = fopen(FILENAME, "r");
    if(!dataFile) {
		syslog(LOG_ERR, "Error opening data file.");
		return -1;
	}

	char buffer[BUFFERSIZE];
	while((bytesRead = fread(buffer, 1, sizeof(buffer), dataFile))){
		if((send(*socketContext->pAcceptConnectionFd, buffer, bytesRead, 0)) != bytesRead) {
			syslog(LOG_ERR, "Error sending file contents over socket connection.");
			return -1;
		}
	}
	fclose(dataFile);
    return 0;
}

int receive(socket_context *pSocketContext) {
	FILE *dataFile = fopen(FILENAME, "a");
    if(!dataFile){
        syslog(LOG_ERR, "Unable to open file for writing.");
        return -1;
    }

    size_t bytesReceived = 0;
	char buffer[BUFFERSIZE];

	while((bytesReceived = recv(*pSocketContext->pAcceptConnectionFd, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytesReceived, dataFile);
        if(memchr(buffer, '\n', bytesReceived) != NULL) {
            break; 
        }
    }
    fclose(dataFile);
    
    // Send back the contents of the file over the socket connection.
    sendFileContentsOverSocket(pSocketContext);
	return 0;
}

int acceptConnections(socket_context *pSocketContext) {
    while(1) {
        if(terminate) {
            // A termination signal was sent. Immediately break out of this loop and exit the function.
            return 0;
        }
        *pSocketContext->pSockAddrStorageSize = sizeof(*pSocketContext->pSockAddrStorage);
		int acceptedConnectionFd = accept(*pSocketContext->pSocketFd, (struct sockaddr *) pSocketContext->pSockAddrStorage, pSocketContext->pSockAddrStorageSize);
		if (acceptedConnectionFd == -1){
		    syslog(LOG_ERR, "Eroor accepting connection.");
			continue;
		} else {
            pSocketContext->pAcceptConnectionFd = &acceptedConnectionFd;
            syslog(LOG_USER, "Accepted connection.");
            
            receive(pSocketContext);
        }
    }
    return 0;
}

int cleanup(socket_context *pSocketContext) {
    freeaddrinfo(pSocketContext->pAddrInfo);
    pSocketContext->pAddrInfo = NULL;
    pSocketContext->pHints = NULL;
    pSocketContext->pSockAddrStorage = NULL;
    pSocketContext->pSockAddrStorageSize = NULL;
    pSocketContext->pSocketFd = NULL;
    pSocketContext->pAcceptConnectionFd = NULL;

    free(pSocketContext);
    remove(FILENAME);
    return 0;
}

int main(int argc, char *argv[]) {
    socket_context *pSocketContext = malloc(sizeof(socket_context));
    struct sockaddr_storage sockAddrStorage;
    socklen_t sockAddrStorageSize;
    struct addrinfo hints, *pAddrInfo;
    int socketFd;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    pid_t pid;

    bool runAsDaemon = false;
    openlog(NULL, 0, LOG_USER);
    
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
		free(pSocketContext);
        freeaddrinfo(pAddrInfo);
		syslog(LOG_ERR, "Error creating socket");
		exit(EXIT_FAILURE);
	}

    int sockOpt = 1;
    if(setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &sockOpt, sizeof(sockOpt))) {
        syslog(LOG_ERR,"Could not set socket options.");
        free(pSocketContext);
        freeaddrinfo(pAddrInfo);
        exit(EXIT_FAILURE);
    }

    if(bind(socketFd, pAddrInfo->ai_addr, pAddrInfo->ai_addrlen) == -1) {
        syslog(LOG_ERR, "Could not bind socket to address.");
        free(pSocketContext);
        freeaddrinfo(pAddrInfo);
        exit(EXIT_FAILURE);
    }

    if(listen(socketFd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Could not invoke listen on socket.");
    }
    sockAddrStorageSize = sizeof(sockAddrStorage);
    
    pSocketContext->pSocketFd = &socketFd;
    pSocketContext->pSockAddrStorage = &sockAddrStorage;
    pSocketContext->pSockAddrStorageSize = &sockAddrStorageSize;
    pSocketContext->pHints = &hints;
    pSocketContext->pAddrInfo = pAddrInfo;

    if(runAsDaemon) {
        pid = fork();
        if (pid == -1) {
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
        int fd = open("/dev/null", O_WRONLY | O_CREAT, 0666);
        dup2(fd, 1);

        // Begin accepting connections and data.
        acceptConnections(pSocketContext);
        syslog(LOG_USER, "Closing connections and cleaning up.");
        cleanup(pSocketContext);
        
        if(fd != -1) {
            close(fd);
        }

        if(socketFd != -1) {
            close(socketFd);
        }
        
        closelog();
        exit(EXIT_SUCCESS);
    }

    // Running interactively (not as a daemon).
    syslog(LOG_USER, "Running in interactive mode.");

    // Begin accepting connections and data.
    acceptConnections(pSocketContext);
    syslog(LOG_USER, "Closing connections and cleaning up.");
    cleanup(pSocketContext);
    if(socketFd != -1) {
        close(socketFd);
    }
    closelog();
    exit(EXIT_SUCCESS);
}
