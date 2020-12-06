#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "debug.h"
#include "protocol.h"
#include "server.h"
#include "client_registry.h"
#include "player_registry.h"
#include "jeux_globals.h"
#include "csapp.h"

#ifdef DEBUG
int _debug_packets_ = 1;
#endif

static void terminate(int status);

void sighup_handler(int sig){
    debug("this is the sighup handler");
    terminate(0); //for now terminate with success
}

/*
 * "Jeux" game server.
 *
 * Usage: jeux <port>
 */
int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.

    // Perform required initializations of the client_registry and
    // player_registry.
    client_registry = creg_init();
    player_registry = preg_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function jeux_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.
    struct sigaction new_action;
    new_action.sa_handler = sighup_handler;
    new_action.sa_flags = 0;
    sigaction(SIGHUP, &new_action, NULL);



    // fprintf(stderr, "You have to finish implementing main() "
	   //  "before the Jeux server will function.\n\n");
    if(argc != 3 || strcmp(argv[1], "-p")) {
        printf("Proper call is jeux -p <port>\n");
        terminate(EXIT_FAILURE);
    } // dont forget to error handle the port number

    int listenfd, *connfd;
    int port = atoi(argv[2]); //the port number
    socklen_t clientlen = sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid;

    listenfd = Open_listenfd(port);
    debug("server now waiting on port %d", port);
    while(1) { //main loop to accept connections (needs to be improved as in textbook)
        connfd = Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA*) &clientaddr, &clientlen);
        Pthread_create(&tid, NULL, jeux_client_service, (void*)connfd);
    }



    terminate(EXIT_FAILURE);
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);

    debug("%ld: Waiting for service threads to terminate...", pthread_self());
    creg_wait_for_empty(client_registry);
    debug("%ld: All service threads terminated.", pthread_self());

    // Finalize modules.
    creg_fini(client_registry);
    preg_fini(player_registry);

    debug("%ld: Jeux server terminating", pthread_self());
    exit(status);
}
