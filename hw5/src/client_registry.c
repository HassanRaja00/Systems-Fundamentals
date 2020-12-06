#include <semaphore.h>

#include "client_registry.h"
#include "csapp.h"
#include "client.h"
#include "player.h"
#include "debug.h"

typedef struct client_registry {
	//volatile tells compiler that the value of cnt can change at any time
	volatile int count;
	CLIENT *connections[MAX_CLIENTS]; //will hold the file descriptors of clients
	sem_t mutex;
	sem_t pin;
	sem_t cli;
} CLIENT_REGISTRY;


CLIENT_REGISTRY *creg_init() {
	CLIENT_REGISTRY *cr = malloc(sizeof(CLIENT_REGISTRY));
	if(cr == NULL)
		return NULL; //meaning malloc failed
	cr->count = 0;
	Sem_init(&cr->mutex, 0, 1); // mutex = 1
	Sem_init(&cr->pin, 0, MAX_CLIENTS);
	Sem_init(&cr->cli, 0, 0);
	debug("## created client_registry ##");
	return cr;
}


void creg_fini(CLIENT_REGISTRY *cr) {
	if(cr->count != 0){
		printf("There are still clients registered! Unregister them first.\n\n");
		return;
	}
	debug("#### freeing client_registry ####");
	free(cr);
}


CLIENT *creg_register(CLIENT_REGISTRY *cr, int fd) {
	P(&cr->pin);
	P(&cr->mutex);
	if(cr->count == MAX_CLIENTS){
		debug("max num of clients received!");
		return NULL; // reached max
	}
	CLIENT *cli = client_create(cr, fd);
	if(cli == NULL){
		return NULL; //client creation failed
	}
	cr->connections[cr->count] = cli;
	cr->count = cr->count + 1;
	V(&cr->mutex); //lock mutex
	V(&cr->cli);

	return cli;

}


int creg_unregister(CLIENT_REGISTRY *cr, CLIENT *client) {
	P(&cr->cli);
	P(&cr->mutex);
	//first check if this client exists
	int cli_fd = client_get_fd(client);
	int i, exists = 0;
	for(i = 0; i < cr->count; i++){
		int temp = client_get_fd(cr->connections[i]);
		if(temp == cli_fd){
			exists = 1;
			break;
		}
	}
	if(!exists){
		debug("this client is not registered!");
		return -1;
	}

	client_unref(client, "creg_unregister"); //decrease client ref count by 1
	cr->connections[i] = cr->connections[cr->count-1]; //swap with last elm
	cr->count = cr->count - 1;
	V(&cr->mutex);
	V(&cr->pin);

	return 0;
}



CLIENT *creg_lookup(CLIENT_REGISTRY *cr, char *user) {
	//check if username exists
	int i, exists = 0;
	for(i = 0; i < cr->count; i++){
		PLAYER *player = client_get_player(cr->connections[i]);
		if(player == NULL){
			continue;
		}
		if(!strcmp(player_get_name(player), user)){
			exists = 1;
			break;
		}
	}
	if(!exists){
		debug("no player with this name!");
		return NULL;
	}

	return cr->connections[i]; //return the client registered under this username

}



/*
 * It is the caller's
 * responsibility to decrement the reference count of each of the
 * entries and to free the array when it is no longer needed.
 */
PLAYER **creg_all_players(CLIENT_REGISTRY *cr) {
	int count = 0;
	PLAYER ** players = malloc(sizeof(PLAYER*) * (count+1));
	for(int i = 0; i < cr->count; i++) {
		PLAYER *temp = client_get_player(cr->connections[i]);
		if(temp == NULL){
			continue;
		}
		player_ref(temp, "adding to players list in client_registry"); //ref count increases
		players[count] = temp;
		count++;
		players = realloc(players, sizeof(PLAYER*) * (count+1));
	}
	players[count] = NULL; //last element is NULL

	return players;

}


void creg_wait_for_empty(CLIENT_REGISTRY *cr) {
	debug("## waiting for empty ##");
	P(&cr->mutex);
	while(cr->count > 0);
	V(&cr->mutex);
	debug("## now is empty ##");
	return;

}


/*
 * Shut down (using shutdown(2)) all the sockets for connections
 * to currently registered clients.  The clients are not unregistered
 * by this function.
 */
void creg_shutdown_all(CLIENT_REGISTRY *cr) {
	P(&cr->mutex); //not sure if we have to use this
	int i;
	for(i = 0; i< cr->count; i++){
		if(shutdown(client_get_fd(cr->connections[i]), SHUT_RD) == -1){
			debug("error in shutdown for client %d", i);
		}
	}
	V(&cr->mutex);
}