

#include "client_registry.h"
#include "invitation.h"
#include "csapp.h"
#include "game.h"
#include "client.h"
#include "debug.h"

typedef struct invitation{
	volatile int count;
	CLIENT *src;
	CLIENT *target;
	INVITATION_STATE state;
	GAME *game;
	GAME_ROLE src_role;
	GAME_ROLE target_role;
	sem_t mutex;
} INVITATION;


INVITATION *inv_create(CLIENT *source, CLIENT *target,
		       GAME_ROLE source_role, GAME_ROLE target_role) {
	if(source == target){
		return NULL;
	}
	INVITATION *inv = malloc(sizeof(INVITATION));
	if(inv == NULL){
		return NULL;
	}
	debug(" #### starting creation of inv ####");
	inv->count = 1;
	inv->src = source;
	client_ref(source, "adding source to invite");
	inv->target = target;
	client_ref(target, "adding target to invite");
	inv->state = INV_OPEN_STATE;
	inv->game = NULL;
	inv->src_role = source_role;
	inv->target_role = target_role;
	Sem_init(&inv->mutex, 0, 1); // mutex = 1
	debug(" #### finished creating ####");

	return inv;
}


INVITATION *inv_ref(INVITATION *inv, char *why) {
	P(&inv->mutex); //unlock
	inv->count = inv->count +1;
	debug("## increasing inv ref from %d -> %d ##", inv->count-1, inv->count);
	debug("%s", why);
	V(&inv->mutex); //lock
	return inv;
}


void inv_unref(INVITATION *inv, char *why) {
	if(inv == NULL || inv->count == 0){
		debug("### error in unref inv ###");
		return;
	}
	P(&inv->mutex);
	inv->count = inv->count - 1;
	debug("## decreasing inv ref from %d -> %d ##", inv->count+1, inv->count);
	debug("%s", why);
	V(&inv->mutex);
	if(inv->count == 0){
		debug("### freeing an inv ###");
		free(inv);
		inv = NULL;
	}


}


CLIENT *inv_get_source(INVITATION *inv) {
	return inv->src;
}


CLIENT *inv_get_target(INVITATION *inv) {
	return inv->target;
}


GAME_ROLE inv_get_source_role(INVITATION *inv) {
	return inv->src_role;
}


GAME_ROLE inv_get_target_role(INVITATION *inv) {
	return inv->target_role;
}


GAME *inv_get_game(INVITATION *inv) {
	return inv->game;
}


int inv_accept(INVITATION *inv) {
	if(inv->state != INV_OPEN_STATE) {
		debug("### error in inv_accept ###");
		return -1;
	}
	P(&inv->mutex);
	inv->state = INV_ACCEPTED_STATE;
	inv->game = game_create();
	V(&inv->mutex);
	return 0;
}


/*
 * Close an INVITATION, changing it from either the OPEN state or the
 * ACCEPTED state to the CLOSED state.  If the INVITATION was not previously
 * in either the OPEN state or the ACCEPTED state, then it is an error.
 * If INVITATION that has a GAME in progress is closed, then the GAME
 * will be resigned by a specified player.
 *
 * @param inv  The INVITATION to be closed.
 * @param role  This parameter identifies the GAME_ROLE of the player that
 * should resign as a result of closing an INVITATION that has a game in
 * progress.  If NULL_ROLE is passed, then the invitation can only be
 * closed if there is no game in progress.
 * @return 0 if the INVITATION was successfully closed, otherwise -1.
 */
int inv_close(INVITATION *inv, GAME_ROLE role) {
	P(&inv->mutex);
	if(inv->state == INV_OPEN_STATE || inv->state == INV_ACCEPTED_STATE) {
		if(role == NULL_ROLE && inv->game != NULL) {
			if(game_is_over(inv->game) == 0){
				debug("### game still in progress ###");
				V(&inv->mutex);
				return -1;
			}
		}
		inv->state = INV_CLOSED_STATE;
		if(inv->game != NULL) {
			game_resign(inv->game, role);
		}
	}
	else {
		debug("############ not in open or accepted state ############");
		return -1;
	}
	V(&inv->mutex);

	return 0;
	}

