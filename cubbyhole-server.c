#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


#ifndef CUBBYSERVER
#define CUBBYSERVER

/**
 * Defines the maximum length a string in e.g.
 * a request can take in this server implementation.
 */
#define MAX_STRING 256

#endif



/* --- Global variables --- */

/* Easier addressable response array. */
typedef enum {
    WELCOME,
    HELP,
    DROP,
    GET,
    LOOK,
    PUT,
    QUIT,
    NOT_SUPPORTED,
    NO_MESSAGE,
    PROMPT
} respType;

/* Predefined responses. */
const char *responses[] = {
    "!HELLO: Welcome to the Cubbyhole Server! Try 'help' for a list of commands",
    "!HELP:\nThe following commands are supported by this Cubbyhole:\n\nPUT <message>\t- Places a new message in the cubbyhole\nGET\t\t- Takes the message out of the cubbyhole and displays it\nLOOK\t\t- Displays the massage without taking it out of the cubbyhole\nDROP\t\t- Takes the message out of the cubbyhole without displaying it\nHELP\t\t- Displays this help message\nQUIT\t\t- Terminates the connection\n",
    "!DROP: ok",
    "!GET: ",
    "!LOOK: ",
    "!PUT: ok",
    "!QUIT: ok",
    "!NOT SUPPORTED",
    "<no message stored>",
    "\n> "
};

/* Descriptors for server and client. */
int socketFD;
int clientFD;

/* Message storage variable. */
volatile unsigned char cubby[MAX_STRING];

/* Empty cubby indicator. 0 = empty, !0 = full. */
volatile unsigned int empty;

/* Lock variable for changing both shared variables. */
pthread_mutex_t cubbyLock;

/* Argument struct to pass to worker thread. */
typedef struct {
    int clientFD;
} workerArgs;



/* --- Functions --- */


/**
 * Cleans up used data structures and
 * returns a success code.
 */
void quitHandler() {

    printf("\n\nShutting down...\n");

    /* Close server socket descriptor. */
    close(socketFD);

    if(clientFD != -1) {
        close(clientFD);
    }

    printf("Goodbye!\n");

    /* Return with a success code. */
    exit(EXIT_SUCCESS);
}


/**
 * Print the usage of server executable.
 */
void printUsage(char *commandName) {
    printf("Usage: %s [PORT]\n", commandName);
    printf("Spawns a server capable of interpreting cubbyhole commands.\n");
}


/**
 * Sends result string to client.
 * Optionally includes payload if needed for command.
 * If incPayload = 0: include payload, else: do not include.
 */
void sendResponse(int fd, respType TYPE, unsigned int incPayload, unsigned char (*payload)[MAX_STRING]) {

    /* First send command confirmation. */
    send(fd, responses[TYPE], strlen(responses[TYPE]), 0);

    /* If payload should be sent (GET, LOOK) - include it. */
    if(incPayload == 0) {
        send(fd, *payload, strlen((const char *) (*payload)), 0);
    }

    /* Send next line prompt. */
    send(fd, responses[PROMPT], strlen(responses[PROMPT]), 0);
}


/**
 * Awaits command from client and normalizes it.
 * Means: command contains only upper case letters
 * after this function.
 */
unsigned char * getCommand(int fd, unsigned char (*req)[MAX_STRING], unsigned char (*tmpReq)[MAX_STRING]) {

    unsigned int i, u;

    /* Zero both structures. */
    memset(*req, 0, sizeof(*req));
    memset(*tmpReq, 0, sizeof(*tmpReq));

    /* Receive command from client. */
    recv(fd, *tmpReq, sizeof(*tmpReq), 0);

    u = 0;

    for(i = 0; (*tmpReq)[i]; i++) {

        /**
         * Turn request into upper case only mode.
         * Copy to real command if not a control character
         * (newline, carriage return).
         */
        if(((*tmpReq)[i] != 10) && ((*tmpReq)[i] != 13)) {
            (*req)[u] = toupper((*tmpReq)[i]);
            u++;
        }
    }

    return *req;
}


/**
 * Extracts the cubby (message) part from a PUT request.
 */
void extractPayload(unsigned char (*tmpReq)[MAX_STRING], unsigned char (*payload)[MAX_STRING]) {

    unsigned int i, u;

    /* Zero payload structure. */
    memset(*payload, 0, sizeof(*payload));

    u = 0;

    for(i = 4; (*tmpReq)[i]; i++) {

        /* Copy to payload if not a control character (newline, carriage return). */
        if(((*tmpReq)[i] != 10) && ((*tmpReq)[i] != 13)) {
            (*payload)[u] = (*tmpReq)[i];
            u++;
        }
    }
}


/**
 * Main worker function.
 * This is the function newly dispatched threads
 * enter first. It handles everything a lifetime
 * of a client connection concerns.
 */
void * handleRequests(void *args) {

    /* Reserve space for incoming request and temporary save. */
    unsigned char req[MAX_STRING];
    unsigned char tmpReq[MAX_STRING];
    unsigned char payload[MAX_STRING];

    unsigned char *command;
    unsigned int length;

    int workerFD;

    /* Restore passed arguments. */
    workerArgs *wArgs;
    wArgs = (workerArgs *) args;
    workerFD = wArgs->clientFD;

    /**
     * This signals the pthread library that no other
     * thread will join onto this thread. As soon as
     * this thread exits, the resources can be free'd.
     */
    pthread_detach(pthread_self());

    /* Zero payload structure. */
    memset(payload, 0, sizeof(payload));

    /* Send the !HELLO welcome string to client. */
    sendResponse(workerFD, WELCOME, 1, &payload);

    /* Get normalized command. */
    command = getCommand(workerFD, &req, &tmpReq);
    length = strlen((const char *) command);

    while((strncmp((const char *) command, "QUIT", 4) != 0) || (length != 4)) {

        /* Zero payload structure. */
        memset(payload, 0, sizeof(payload));

        /* Check which command we received. */
        if((strncmp((const char *) command, "HELP", 4) == 0) && (length == 4)) {

            printf("HELP requested.\n");

            /* Send the !HELP string to client. */
            sendResponse(workerFD, HELP, 1, &payload);
        }
        else if((strncmp((const char *) command, "DROP", 4) == 0) && (length == 4)) {

            /* If cubby is not empty - empty it. */
            if(empty != 0) {

                /* Request to access shared variables. */
                pthread_mutex_lock(&cubbyLock);

                printf("DROP requested, cubby not empty. Dropping: %s.\n", (char *) cubby);
                memset((void *) cubby, 0, sizeof(cubby));
                empty = 0;

                /* Release access to shared variables. */
                pthread_mutex_unlock(&cubbyLock);
            }
            else {
                printf("DROP requested, cubby empty.\n");
            }

            /* Send the !DROP string to client. */
            sendResponse(workerFD, DROP, 1, &payload);
        }
        else if((strncmp((const char *) command, "GET", 3) == 0) && (length == 3)) {

            /* Set default payload value. */
            memcpy(payload, responses[NO_MESSAGE], strlen(responses[NO_MESSAGE]));

            /* If cubby not empty - redirect result string. */
            if(empty != 0) {

                memset(payload, 0, sizeof(payload));

                /* Request to access shared variables. */
                pthread_mutex_lock(&cubbyLock);

                strncpy((char *) payload, (const char *) cubby, strlen((const char *) cubby));
                payload[255] = 0;

                printf("GET requested, cubby not empty. Getting and emptying: %s.\n", payload);

                /* After getting, delete cubby. */
                memset((void *) cubby, 0, sizeof(cubby));
                empty = 0;

                /* Release access to shared variables. */
                pthread_mutex_unlock(&cubbyLock);
            }
            else {
                printf("GET requested, cubby empty. Sending: %s.\n", payload);
            }

            /* Send the !GET string and cubby (if available) to client. */
            sendResponse(workerFD, GET, 0, &payload);
        }
        else if((strncmp((const char *) command, "LOOK", 4) == 0) && (length == 4)) {

            /* Set default payload value. */
            memcpy(payload, responses[NO_MESSAGE], strlen(responses[NO_MESSAGE]));

            /* If cubby not empty - copy contents to payload. */
            if(empty != 0) {

                memset(payload, 0, sizeof(payload));

                /* Request to access shared variables. */
                pthread_mutex_lock(&cubbyLock);

                strncpy((char *) payload, (const char *) cubby, strlen((const char *) cubby));
                payload[255] = 0;

                /* Release access to shared variables. */
                pthread_mutex_unlock(&cubbyLock);

                printf("LOOK requested, cubby not empty. Sending: %s.\n", payload);
            }
            else {
                printf("LOOK requested, cubby empty. Sending: %s.\n", payload);
            }

            /* Send the !LOOK string and cubby (if available) to client. */
            sendResponse(workerFD, LOOK, 0, &payload);
        }
        else if(strncmp((const char *) command, "PUT", 3) == 0) {

            /* Retrieve the message part from user request. */
            extractPayload(&tmpReq, &payload);

            /* Request to access shared variables. */
            pthread_mutex_lock(&cubbyLock);

            /* Overwrite cubby and set empty to false. */
            memset((void *) cubby, 0, sizeof(cubby));
            strncpy((char *) cubby, (const char *) payload, strlen((const char *) payload));
            cubby[255] = 0;
            empty = 1;

            printf("PUT requested. New cubby: %s.\n", (char *) cubby);

            /* Release access to shared variables. */
            pthread_mutex_unlock(&cubbyLock);

            /* Send the !PUT string to client. */
            sendResponse(workerFD, PUT, 1, &payload);
        }
        else {
            /* Send the !NOT SUPPORTED string to client. */
            sendResponse(workerFD, NOT_SUPPORTED, 1, &payload);
        }

        /* Get normalized command. */
        command = getCommand(workerFD, &req, &tmpReq);
        length = strlen((const char *) command);

        /* If pipe is broken, quit client. */
        if(length == 0) {
            command = (unsigned char *) "QUIT";
            length = 4;
            printf("Broken pipe. Force close.\n");
        }
    }

    /* Zero payload structure. */
    memset(payload, 0, sizeof(payload));

    /* Send the !QUIT string to client. */
    sendResponse(workerFD, QUIT, 1, &payload);

    printf("QUIT\n");

    /* Close client descriptor. */
    close(workerFD);

    /* Free thread arguments resource. */
    free(wArgs);

    /* Exit thread. */
    pthread_exit(NULL);
}



/* --- Main --- */


/**
 * Parse arguments, open up a TCP socket, listen for
 * requests, respond according to cubbyhole protocol
 * and close connections.
 */
int main(int argc, char **argv) {

    /**
     * Structure represting a pthread, in this case
     * the connection worker handler. */
    pthread_t workerThread;

    /* Placeholder for worker arguments. */
    workerArgs *wArgs;

    /* Host information for server and connecting clients. */
    struct sockaddr_in server;
    struct sockaddr_in client;

    socklen_t hostStructSize;

    /* Enable value for port reuse. */
    unsigned int portReuse = 1;

    /* Initialize mutex variable for shared variables. */
    pthread_mutex_init(&cubbyLock, NULL);

    /* Initialize data structures. */
    hostStructSize = sizeof(struct sockaddr_in);
    memset(&server, 0, hostStructSize);

    /**
     * Register clean up function on CTRL + C (SIGINT).
     * Also broken pipes will be ignored so that we can
     * handle them independently.
     */
    signal(SIGINT, quitHandler);
    signal(SIGPIPE, SIG_IGN);


    /* Request to access shared variables. */
    pthread_mutex_lock(&cubbyLock);

    /* Initially set cubby and empty indicator to default values. */
    memset((void *) cubby, 0, sizeof(cubby));
    empty = 0;

    /* Release access to shared variables. */
    pthread_mutex_unlock(&cubbyLock);


    /**
     * Catch for too few input parameters.
     * Two required, last one being port to run on.
     * Returns failure code.
     */
    if(argc != 2) {
        printUsage(argv[0]);

        return EXIT_FAILURE;
    }

    /* Check supplied port for out of bounds value. */
    if((((unsigned int) atoi(argv[1])) < 1) || (((unsigned int) atoi(argv[1])) > 65535)) {
        printf("Port number is either too small or too big. Please choose a number between 1 and 65535, for security reason consider a port above 1023.\n");

        return EXIT_FAILURE;
    }

    /* Specify connection type, address and port in network order. */
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons((unsigned int) atoi(argv[1]));


    /* Create a TCP socket. */
    socketFD = socket(AF_INET, SOCK_STREAM, 0);

    /* Catch socket binding errors. */
    if(socketFD == -1) {
        printf("Socket initialization went wrong. Terminating.\n");

        return EXIT_FAILURE;
    }

    /* Now set the option to reuse ports. */
    setsockopt(socketFD, SOL_SOCKET, SO_REUSEPORT, &portReuse, sizeof(portReuse));

    /* Bind local socket to file descriptor. */
    if(bind(socketFD, (struct sockaddr *) &server, sizeof(struct sockaddr)) == -1) {
        printf("Binding to local address went wrong. Terminating.\n");

        return EXIT_FAILURE;
    }

    /**
     * Listen on set up socket with allowing the maximum number
     * of clients supported to connect.
     */
    if(listen(socketFD, SOMAXCONN) == -1) {
        printf("Listening on the bound port went wrong. Terminating.\n");

        return EXIT_FAILURE;
    }

    printf("Cubbyhole server listening on port %i.\n\n", ntohs(server.sin_port));


    while(1) {

        /* On an incoming request on socketFD, accept it and fill client structure. */
        clientFD = accept(socketFD, (struct sockaddr *) &client, &hostStructSize);

        if(clientFD == -1) {
            printf("Client descriptor creation went wrong. Terminating.\n");

            return EXIT_FAILURE;
        }

        /* We got a client connection. Pass on to worker thread. */
        wArgs = (workerArgs *) malloc(sizeof(workerArgs));
        wArgs->clientFD = clientFD;

        /* Create a new thread and pass on arguments. */
        if(pthread_create(&workerThread, NULL, handleRequests, (void *) wArgs) == 0) {
            printf("Client connected. Thread dispatched.\n");
        }
        else {
            /* Thread creation error. Terminate. */
            free(wArgs);
            close(clientFD);
            close(socketFD);
            pthread_exit(NULL);
        }
    }
}