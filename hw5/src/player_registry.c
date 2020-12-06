#include <semaphore.h>

#include "player_registry.h"
#include "player.h"
#include "csapp.h"
#include "debug.h"

typedef struct player_registry{
	volatile int count;
	PLAYER *players[250];
	sem_t mutex;
} PLAYER_REGISTRY;


PLAYER_REGISTRY *preg_init(void) {
	PLAYER_REGISTRY *pr = malloc(sizeof(PLAYER_REGISTRY));
	if(pr == NULL){
		return NULL;
	}
	pr->count = 0;
	Sem_init(&pr->mutex, 0, 1);
	return pr;
}


void preg_fini(PLAYER_REGISTRY *preg) {
	if(preg->count > 0){
		for(int i = 0; i < preg->count; i++) {
			player_unref(preg->players[i], " ### freeing player_registry ###");
		}
	}
	debug("#### freeing player_registry ####");
	free(preg);
}


PLAYER *preg_register(PLAYER_REGISTRY *preg, char *name) {
	P(&preg->mutex);
	int i;
	for(i = 0; i < preg->count; i++) {
		if(!strcmp(name, player_get_name(preg->players[i]))) {
			debug(" ### found duplicate name ###");
			player_ref(preg->players[i], " ### returning duplicate ###");
			V(&preg->mutex);
			return preg->players[i];
		}
	}

	PLAYER *p = player_create(name);
	player_ref(p, "### registering player ###");
	preg->players[preg->count] = p;
	preg->count = preg->count + 1;
	V(&preg->mutex);
	debug("### finished preg_register ###");
	return p;
}