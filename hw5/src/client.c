#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#include "client_registry.h"
#include "client.h"
#include "csapp.h"
#include "invitation.h"
#include "debug.h"

typedef struct client {
	volatile int count; //ref cnt
	int fd;
	int logged_in;
	PLAYER *player;
	INVITATION *invs[150];
	int inv_count;
	CLIENT_REGISTRY *cr;
	sem_t mutex;
} CLIENT;


CLIENT *client_create(CLIENT_REGISTRY *creg, int fd) {
	CLIENT *cli = malloc(sizeof(CLIENT));
	if(cli == NULL){
		return NULL;
	}
	cli->fd = fd;
	cli->count = 1;
	cli->logged_in = 0;
	cli->player = NULL;
	cli->inv_count = 0;;
	cli->cr = creg;
	Sem_init(&cli->mutex, 0, 1);
	return cli;
}


CLIENT *client_ref(CLIENT *client, char *why) {
	P(&client->mutex);
	client->count = client->count + 1;
	debug("### client ref increased from %d -> %d", client->count-1, client->count);
	debug("%s", why);
	V(&client->mutex);
	return client;
}


void client_unref(CLIENT *client, char *why) {
	if(client->count == 0 || client == NULL){
		debug("###count would go negative ###");
		return;
	}
	P(&client->mutex);
	client->count = client->count - 1;
	debug("### client ref decreased from %d -> %d", client->count+1, client->count);
	debug("%s", why);
	V(&client->mutex);
	if(client->count == 0){
		debug("## freeing a client ###");
		//free all invitations first
		// for(int i = 0; i < client->inv_count; i++){
		// 	if(client->invs[i] != NULL)
		// 		inv_unref(client->invs[i], "freeing client inv refs");
		// }
		// debug("#### freed invs ####");
		free(client);
		client = NULL;
		debug("#### freed client ###");
	}
}


int client_login(CLIENT *client, PLAYER *player) {
	if(client->logged_in == 1 || creg_lookup(client->cr, player_get_name(player)) != NULL){
		return -1;
	}
	P(&client->mutex);
	client->player = player;
	player_ref(player, "logging in client");
	client->logged_in = 1;
	V(&client->mutex);
	return 0;
}
// if(inv_get_source(client->invs[id]) == client){
// 		role = inv_get_source_role(client->invs[id]);
// 		other = inv_get_target(client->invs[id]);
// 		other_role = inv_get_target_role(client->invs[id]);
// 	}
// 	else {
// 		role = inv_get_target_role(client->invs[id]);
// 		other = inv_get_source(client->invs[id]);
// 		other_role = inv_get_source_role(client->invs[id]);
// 	}


int client_logout(CLIENT *client) {
	if(client->logged_in == 0){
		return -1;
	}
	P(&client->mutex);
	player_unref(client->player, "client logging out");
	client->player = NULL;
	client->logged_in = 0;
	V(&client->mutex);
	// any invs/games are revoked/resigned
	// CLIENT *other;
	for(int i = 0; i < client->inv_count; i++) {
		INVITATION *inv = client->invs[i];
		GAME *game = inv_get_game(inv);
		if(inv == NULL){
			continue;
		}
		if(inv_get_source(inv) == client){
			// other = inv_get_target(inv);
			if(game != NULL && !game_is_over(inv_get_game(inv))) {
				//if there is a game being played
				if(client_resign_game(client, i)== -1) {
					debug("## source resigning failed ##");
					return -1;
				}
			}
			else if(game == NULL){
				// if the invite was not accepted yet
				if(client_revoke_invitation(client, i) == -1){
					debug("## source revoking failed ##");
					return -1;
				}
			}
		}
		else {
			// other = inv_get_source(inv);
			if(game != NULL && !game_is_over(inv_get_game(inv))){
				//if there is a game, resign
				if(client_resign_game(client, i)== -1) {
					debug("## target resigning failed ##");
					return -1;
				}
			}
			else if(game == NULL){
				//no game yet, so decline
				if(client_decline_invitation(client, i) == -1) {
					debug("### error declining logout ###");
					return -1;
				}
			}

		}
	}
	debug("## successful logout ##");

	return 0;
}


PLAYER *client_get_player(CLIENT *client) {
	if(client->logged_in == 0){
		return NULL;
	}
	return client->player;
}


int client_get_fd(CLIENT *client) {
	return client->fd;
}


int client_send_packet(CLIENT *player, JEUX_PACKET_HEADER *pkt, void *data) {
	P(&player->mutex);
	int res = proto_send_packet(player->fd, pkt, data);
	if(res == -1) {
		V(&player->mutex);
		return -1;
	}
	V(&player->mutex);

	return 0;
}


int client_send_ack(CLIENT *client, void *data, size_t datalen) {
	P(&client->mutex);
	JEUX_PACKET_HEADER header;
	header.type = JEUX_ACK_PKT;
	header.id = 0;
	header.role = 0;
	if(data == NULL || datalen == 0){
		header.size = 0;
	}
	else {
		header.size = htons(datalen);
	}
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	header.timestamp_sec = htonl((uint32_t) time.tv_sec);
	header.timestamp_nsec = htonl((uint32_t) time.tv_nsec);
	if(proto_send_packet(client->fd, &header, data) == -1){
		V(&client->mutex);
		return -1;
	}
	V(&client->mutex);

	return 0;
}


int client_send_nack(CLIENT *client) {
	P(&client->mutex);
	JEUX_PACKET_HEADER header;
	header.type = JEUX_NACK_PKT;
	header.id = 0;
	header.role = 0;
	//i dont think nack needs a body??
	header.size = 0;
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	header.timestamp_sec = htonl((uint32_t) time.tv_sec);
	header.timestamp_nsec = htonl((uint32_t) time.tv_nsec);
	int res = proto_send_packet(client->fd, &header, NULL);
	if(res == -1){
		V(&client->mutex);
		return -1;
	}
	V(&client->mutex);
	return 0;
}


int client_add_invitation(CLIENT *client, INVITATION *inv) {
	P(&client->mutex);
	int id = client-> inv_count;
	client->invs[client->inv_count] = inv_ref(inv, " ### adding invitation ###");
	// inv count is also the ID
	client->inv_count = client->inv_count + 1;
	V(&client->mutex);
	return id;
}


int client_remove_invitation(CLIENT *client, INVITATION *inv){
	P(&client->mutex);
	int exists = 0, id;
	for(id = 0; id < client->inv_count; id++) {
		if(inv_get_game(inv) == inv_get_game(client->invs[id])){ //same game
			exists = 1;
			break;
		}
	}
	if(!exists) {
		V(&client->mutex);
		debug("### inv does not exist ###");
		return -1;
	}
	inv_unref(client->invs[id], "### removing inv ###");
	V(&client->mutex);
	return id;
}


int client_make_invitation(CLIENT *source, CLIENT *target,
			   GAME_ROLE source_role, GAME_ROLE target_role) {
	INVITATION *inv = inv_create(source, target, source_role, target_role);
	if(inv == NULL){
		debug("### error in creating invitation ###");
		return -1;
	}
	int source_id = client_add_invitation(source, inv);
	if(source_id == -1){
		return -1;
	}
	int target_id = client_add_invitation(target, inv);
	if(target_id == -1){
		client_remove_invitation(source, inv);
		return -1;
	}
	char *source_name = player_get_name(source->player);
	debug("### source name is %s ###", source_name);

	P(&target->mutex);
	JEUX_PACKET_HEADER invited_packet;
	invited_packet.type = JEUX_INVITED_PKT;
	invited_packet.id = target_id;
	invited_packet.role = target_role;
	invited_packet.size = htons(strlen(source_name));
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	invited_packet.timestamp_sec = htonl((uint32_t) time.tv_sec);
	invited_packet.timestamp_nsec = htonl((uint32_t) time.tv_nsec);
	if(proto_send_packet(target->fd, &invited_packet, source_name) == -1) {
		V(&target->mutex);
		client_remove_invitation(source, inv);
		client_remove_invitation(target, inv);
		return -1;
	}
	V(&target->mutex);
	return source_id;
}


int client_revoke_invitation(CLIENT *client, int id) {
	if(client->invs[id] == NULL || client != inv_get_source(client->invs[id])) {
		debug(" ### error revoke ###");
		return -1;
	}
	CLIENT *target = inv_get_target(client->invs[id]);
	int exists = 0, i;
	for(i = 0; i < target->inv_count; i++) {
		if(target->invs[i] == client->invs[id]){
			exists = 1; // check inv state by checking if there is a game
			if(inv_get_game(client->invs[id]) == NULL){
				if(inv_close(client->invs[id], NULL_ROLE) == -1){
					debug(" ### error revoke ###");
					return -1;
				}
			}
			else {
				debug(" ### game exists error revoke ###");
				return -1;
			}
			break;
		}
	}
	if(!exists){ // inv dne in the target
		debug(" ### error revoke ###");
		return -1;
	}
	GAME_ROLE target_role = inv_get_target_role(client->invs[id]);
	if(client_remove_invitation(client, client->invs[id]) == -1){
		return -1;
	}
	if(client_remove_invitation(target, target->invs[i]) == -1){
		return -1;
	}
	P(&target->mutex);
	JEUX_PACKET_HEADER revoke;
	revoke.type = JEUX_REVOKED_PKT;
	revoke.id = i;
	revoke.role = target_role;
	revoke.size = 0; //idk again
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	revoke.timestamp_sec = htonl((uint32_t) time.tv_sec);
	revoke.timestamp_nsec = htonl((uint32_t) time.tv_nsec);
	if(proto_send_packet(target->fd, &revoke, NULL) == -1) {
		V(&target->mutex);
		return -1;
	}
	V(&target->mutex);
	debug("### successful revoke ###");
	return 0;
}


int client_decline_invitation(CLIENT *client, int id) {
	if(client->invs[id] == NULL || client != inv_get_target(client->invs[id])) {
		debug("### error in decline ###");
		return -1;
	}
	CLIENT *src = inv_get_source(client->invs[id]);
	int exists = 0, i;
	for(i = 0; i < src->inv_count; i++) {
		if(src->invs[i] == client->invs[id]) {
			if(inv_get_game(client->invs[id]) == NULL) {
				if(inv_close(client->invs[id], NULL_ROLE) == -1){
					debug(" ### error decline ###");
					return -1;
				}
			}
			else {
				debug(" ### rv error decline ###");
				return -1;
			}
			exists = 1;
			break;
		}
	}
	if(!exists){
		debug("### error in decline ###");
		return -1;
	}
	GAME_ROLE src_role = inv_get_source_role(client->invs[id]);
	if(client_remove_invitation(client, client->invs[id]) == -1){
		return -1;
	}
	if(client_remove_invitation(src, src->invs[i]) == -1){
		return -1;
	}
	P(&src->mutex);
	JEUX_PACKET_HEADER decline;
	decline.type = JEUX_DECLINED_PKT;
	decline.id = i;
	decline.role = src_role;
	decline.size = 0;
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	decline.timestamp_sec = htonl((uint32_t) time.tv_sec);
	decline.timestamp_nsec = htonl((uint32_t) time.tv_nsec);
	int res = proto_send_packet(src->fd, &decline, NULL);
	if(res == -1){
		debug("### error in decline ###");
		V(&src->mutex);
		return -1;
	}
	V(&src->mutex);

	return 0;
}


int client_accept_invitation(CLIENT *client, int id, char **strp) {
	debug("### inside client_accept_invitation ###");
	CLIENT *src = inv_get_source(client->invs[id]);
	debug(" ## got source client ##");
	int exists = 0, src_id;
	for(src_id = 0; src_id < src->inv_count; src_id++){
		if(src->invs[src_id] == client->invs[id]) {
			exists = 1;
			break;
		}
	}
	if(!exists) {
		debug(" ### error accept ###");
		return -1;
	}
	debug("## got source inv ##");
	if(inv_accept(client->invs[id]) == -1){ //this function handles ref count
		debug("### unsuccessful accept ###");
		return -1;
	}
	debug("### successful accept ###");
	game_ref(inv_get_game(client->invs[id]), " ### increase ref count of game ###");
	P(&src->mutex);
	// P(&client->mutex);
	JEUX_PACKET_HEADER accepted;
	accepted.type = JEUX_ACCEPTED_PKT;
	accepted.id = src_id;
	accepted.role = 0;
	// debug("# accepted.role %d #", accepted.role);
	if(strp == NULL){
		accepted.size = 0; // accepting is not first move
	}
	else{
		*strp = malloc(sizeof(char)*31);
		*strp = strcpy(*strp, " | | \n-----\n | | \n-----\n | |\n\0");
		debug("##### strp is: %s, %ld #####", *strp, strlen(*strp));
		accepted.size = htons(strlen(*strp)); //accepting is first move
	}
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	accepted.timestamp_sec = htonl((uint32_t) time.tv_sec);
	accepted.timestamp_nsec = htonl((uint32_t) time.tv_nsec);
	if(proto_send_packet(src->fd, &accepted, *strp) == -1){
		V(&src->mutex);
		// V(&client->mutex);
		debug(" ### error accept ###");
		return -1;
	}

	V(&src->mutex);
	// V(&client->mutex);
	debug("#### successfully sent accepted ###");
	inv_unref(client->invs[id], "decreasing");
	return 0;
}


int client_resign_game(CLIENT *client, int id) {
	GAME *game = inv_get_game(client->invs[id]);
	if(game == NULL){
		debug("### game error in resign ###");
		return -1;
	}
	GAME_ROLE role;
	CLIENT *other;
	if(inv_get_source(client->invs[id]) == client){
		role = inv_get_source_role(client->invs[id]);
		other = inv_get_target(client->invs[id]);
	}
	else {
		role = inv_get_target_role(client->invs[id]);
		other = inv_get_source(client->invs[id]);
	}
	if(game_resign(game, role) == -1){
		debug("### resigning error in resign ###");
		return -1;
	}
	if(inv_close(client->invs[id], role) == -1){
		debug("### close error in resign ###");
		return -1;
	}
	int i, exists = 0;
	for(i = 0; i < other->inv_count; i++) {
		if(client->invs[id] == other->invs[i]){
			exists = 1;
			break;
		}
	}
	if(!exists){
		debug(" ### dne error in resign ###");
		return -1;
	}

	if(client_remove_invitation(client, client->invs[id]) == -1){
		return -1;
	}
	if(client_remove_invitation(other, other->invs[i]) == -1){
		return -1;
	}

	P(&other->mutex);
	JEUX_PACKET_HEADER resign;
	resign.type = JEUX_RESIGNED_PKT;
	resign.id = i;
	resign.role = role;
	resign.size = 0;
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	resign.timestamp_sec = htonl((uint32_t) time.tv_sec);
	resign.timestamp_nsec = htonl((uint32_t) time.tv_nsec);
	int res = proto_send_packet(other->fd, &resign, NULL);
	if(res == -1){
		debug("### error in sending resign ###");
		V(&other->mutex);
		return -1;
	}
	V(&other->mutex);

	return 0;
}


int client_make_move(CLIENT *client, int id, char *move) {
	GAME *game = inv_get_game(client->invs[id]);
	if(game ==NULL){
		debug(" #### error no game client_make_move ####");
		return -1;
	}
	GAME_ROLE role;
	GAME_ROLE other_role;
	CLIENT *other;
	int p1;
	if(inv_get_source(client->invs[id]) == client){
		role = inv_get_source_role(client->invs[id]);
		other = inv_get_target(client->invs[id]);
		other_role = inv_get_target_role(client->invs[id]);
	}
	else {
		role = inv_get_target_role(client->invs[id]);
		other = inv_get_source(client->invs[id]);
		other_role = inv_get_source_role(client->invs[id]);
	}
	debug("## %p, %p ##", client, other);
	if(role == FIRST_PLAYER_ROLE && other_role == SECOND_PLAYER_ROLE){
		p1 = 1;
	}
	else if(role == SECOND_PLAYER_ROLE && other_role == FIRST_PLAYER_ROLE){
		p1 = 2;
	}
	int i, exists = 0;
	for(i = 0; i<other->inv_count; i++) {
		if(client->invs[id] == other->invs[i]){
			exists = 1;
			break;
		}
	}
	if(!exists){
		debug("#### error no inv in other client_make_move ####");
		return -1;
	}
	GAME_MOVE *g_move = game_parse_move(game, role, move);
	if(g_move == NULL){
		debug(" #### error parse client_make_move ####");
		return -1;
	}
	if(game_apply_move(game, g_move) == -1) {
		debug(" #### error illegal move client_make_move ####");
		return -1;
	}
	char *new_move = game_unparse_state(game);
	P(&other->mutex);
	JEUX_PACKET_HEADER m;
	m.type = JEUX_MOVED_PKT;
	m.id = i;
	m.role = other_role;
	m.size = htons(strlen(new_move));
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	m.timestamp_sec = htonl((uint32_t) time.tv_sec);
	m.timestamp_nsec = htonl((uint32_t) time.tv_nsec);
	int res = proto_send_packet(other->fd, &m, new_move);
	if(res == -1) {
		debug("### error in sending client_make_move ###");
		V(&other->mutex);
		return -1;
	}
	V(&other->mutex);
	GAME_ROLE winner = game_get_winner(game);
	if(game_is_over(game)) {
		char *result = game_unparse_state(game);
		P(&other->mutex);
		P(&client->mutex);
		JEUX_PACKET_HEADER ended_target;
		JEUX_PACKET_HEADER ended_source;
		ended_target.type = JEUX_ENDED_PKT;
		ended_source.type = JEUX_ENDED_PKT;
		ended_target.id = i;
		ended_source.id = id;
		if(winner == other_role){
			ended_target.role = other_role;
			ended_source.role = other_role;
		} else {
			ended_target.role = role;
			ended_source.role = role;
		}

		ended_target.size = htons(strlen(result));
		ended_source.size = htons(strlen(result));
		struct timespec time;
		clock_gettime(CLOCK_MONOTONIC, &time);
		ended_target.timestamp_sec = htonl((uint32_t) time.tv_sec);
		ended_target.timestamp_nsec = htonl((uint32_t) time.tv_nsec);
		ended_source.timestamp_sec = htonl((uint32_t) time.tv_sec);
		ended_source.timestamp_nsec = htonl((uint32_t) time.tv_nsec);

		if(proto_send_packet(other->fd, &ended_target, result)==-1){
			debug("#### error send ended target ####");
			V(&other->mutex);
			return -1;
		}
		if(proto_send_packet(client->fd, &ended_source, result) == -1) {
			debug("#### error send ended source ####");
			V(&client->mutex);
			return -1;
		}
		V(&other->mutex);
		V(&client->mutex);
		if(client_remove_invitation(client, client->invs[id]) == -1){
			return -1;
		}
		if(client_remove_invitation(other, other->invs[i]) == -1){
			return -1;
		}
		if(winner == FIRST_PLAYER_ROLE && p1 == 1){
			player_post_result(client->player, other->player, 1);
		}
		else if(winner == SECOND_PLAYER_ROLE && p1 == 1){
			player_post_result(client->player, other->player, 2);
		}
		else if(winner == FIRST_PLAYER_ROLE && p1 == 2){
			player_post_result(other->player, client->player, 1);
		}
		else if(winner == SECOND_PLAYER_ROLE && p1 ==2){
			player_post_result(other->player, client->player, 2);
		}
		else{
			player_post_result(client->player, other->player, 0);
		}
	}


	return 0;
}