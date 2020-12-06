#include <semaphore.h>
#include <math.h>

#include "client_registry.h"
#include "player.h"
#include "csapp.h"
#include "debug.h"

typedef struct player{
	volatile int count;
	int rating;
	char name[250];
	sem_t mutex;
} PLAYER;


PLAYER *player_create(char *name) {
	PLAYER *player = malloc(sizeof(PLAYER));
	player->count = 1;
	memcpy(player->name, name, strlen(name));
	player->rating = PLAYER_INITIAL_RATING;
	Sem_init(&player->mutex, 0, 1);
	return player;
}


PLAYER *player_ref(PLAYER *player, char *why) {
	P(&player->mutex);
	player->count = player->count + 1;
	debug("%s", why);
	V(&player->mutex);
	return player;
}


void player_unref(PLAYER *player, char *why) {
	if(player->count == 0 || player == NULL){
		debug("## error player_unref ##");
		return;
	}
	P(&player->mutex);
	player->count = player->count - 1;
	debug("%s", why);
	V(&player->mutex);
	if(player->count == 0) {
		debug("### player freed ###");
		free(player);
		player = NULL;
	}
}


char *player_get_name(PLAYER *player) {
	return player->name;
}


int player_get_rating(PLAYER *player) {
	return player->rating;
}


void player_post_result(PLAYER *player1, PLAYER *player2, int result) {
	P(&player1->mutex);
	P(&player2->mutex);
	int r1 = player1->rating;
	int r2 = player2->rating;
	double e1 = 1/(1 + pow(10,((r2-r1)/400)));
	double e2 = 1/(1 + pow(10,((r1-r2)/400)));
	int s1, s2, p1r, p2r;
	if(result == 0) {
		debug(" ##### tie #####");
		s1 = 0.5, s2 = 0.5;
		int p1r = r1 + 32*(s1-e1);
        int p2r = r2 + 32*(s2-e2);
        player1->rating = p1r;
        player2->rating = p2r;
	}
	else if(result == 1) {
		debug(" ##### player 1 won #####");
		s1 = 1, s2 = 0;
		p1r = r1 + 32*(s1-e1);
        p2r = r2 + 32*(s2-e2);
        player1->rating = p1r;
        player2->rating = p2r;
	}
	else if(result == 2) {
		debug(" ##### player2 won #####");
		s1 = 0, s2 = 1;
		p1r = r1 + 32*(s1-e1);
        p2r = r2 + 32*(s2-e2);
        player1->rating = p1r;
        player2->rating = p2r;
	}
	V(&player1->mutex);
	V(&player2->mutex);
}