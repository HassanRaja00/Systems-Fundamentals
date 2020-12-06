// #include <time.h>
// #include <string.h>

// #include "csapp.h"
// #include "server.h"

// #include "client_registry.h"
// #include "client.h"
// #include "player.h"
// #include "protocol.h"
// #include "debug.h"
// #include "player_registry.h"
// #include "invitation.h"

// #include "jeux_globals.h"


// void *jeux_client_service(void *arg) {
// 	debug("yur");
// 	Pthread_detach(pthread_self());
// 	int connfd = *(int*)arg;
// 	Free(arg);
// 	CLIENT *cli = creg_register(client_registry, connfd);
// 	debug("## registered the client");
// 	int logged_in = 0;

// 	while(1) { //service loop
// 		JEUX_PACKET_HEADER *hdr = Malloc(sizeof(JEUX_PACKET_HEADER));
// 		void *ptr; //this is the username for login
// 		proto_recv_packet(connfd, hdr, &ptr);
// 		debug("received a packet");
// 		uint8_t type = hdr->type;
// 		if(!logged_in && type == JEUX_LOGIN_PKT){
// 			debug("this is a login packet");
// 			logged_in = 1;
// 			int returnVal = 0;
// 			//client_login but need to create a player object too
// 			PLAYER * player = player_create((char*) ptr);
// 			returnVal = client_login(cli, player);
// 			if(returnVal){
// 				debug("unsuccessful login, send a nack packet");
// 				client_send_nack(cli);
// 				continue;
// 			}
// 			debug("successfull login, send ack packet");
// 			client_send_ack(cli, NULL, 0); //error handle later

// 			continue;

// 		}
// 		if((!logged_in && type != JEUX_LOGIN_PKT) || (logged_in && type == JEUX_LOGIN_PKT) ){
// 			debug("sending nack, wrong packet for current state");
// 			client_send_nack(cli);
// 			continue;
// 		}

// 		if(logged_in && type == JEUX_USERS_PKT){
// 			debug("users packet");
// 			//make string of all logged in users
// 			char *res = malloc(sizeof(char));
// 			//do we need a mutex to read
// 			PLAYER **players = creg_all_players(client_registry);
// 			int cnt = 0;
// 			PLAYER *p = players[cnt];
// 			while(p != NULL){
// 				char *username = player_get_name(p);
// 				int rating = player_get_rating(p);
// 				int num_digits = 0;
// 				int temp = rating;
// 				while(temp != 0){
// 					num_digits++;
// 					temp /= 10;
// 				}
// 				char str_rating[num_digits+1];
// 				sprintf(str_rating, "%d", rating); //doesnt need to be thread safe?
// 				res = realloc(res, strlen(username) + num_digits + 3 + strlen(res));
// 				strncat(res, username, strlen(username));
// 				char tab = '\t';
// 				strncat(res, &tab, 1);
// 				strncat(res, str_rating, num_digits);
// 				char nl = '\n';
// 				strncat(res, &nl, 1);
// 				char term = '\0';
// 				strncat(res, &term, 1);

// 				p = players[++cnt];
// 			}
// 			debug("payload string is %s", res);
// 			client_send_ack(cli, (void*) res, strlen(res));
// 			continue;
// 		}

// 		if(logged_in && type == JEUX_INVITE_PKT){
// 			debug("invite packet");
// 			char *target = (char*) ptr;
// 			CLIENT *cli_target;
// 			//check if this user exists
// 			PLAYER ** players = creg_all_players(client_registry);
// 			int cnt = 0, exists = 0;
// 			PLAYER *p = players[cnt];
// 			while(p != NULL) {
// 				char* name = player_get_name(p);
// 				if(!strcmp(name, target)){
// 					exists = 1;
// 					cli_target = creg_lookup(client_registry, target);
// 					break;
// 				}
// 				p = players[++cnt];
// 			}
// 			if(!exists){
// 				debug("other user does not exist");
// 				client_send_nack(cli);
// 				continue;
// 			}

// 			uint8_t role = hdr->role;
// 			if(role != FIRST_PLAYER_ROLE || role != SECOND_PLAYER_ROLE){
// 				//send nack
// 				client_send_nack(cli);
// 				continue;
// 			}

// 			int result;

// 			if(role == FIRST_PLAYER_ROLE) //make inv func handle id differences
// 				result = client_make_invitation(cli, cli_target, FIRST_PLAYER_ROLE, SECOND_PLAYER_ROLE);
// 			else
// 				result = client_make_invitation(cli, cli_target, SECOND_PLAYER_ROLE, FIRST_PLAYER_ROLE);

// 			if(result == -1){
// 				client_send_nack(cli);
// 			}
// 			else{
// 				JEUX_PACKET_HEADER ack;
// 				ack.type = JEUX_ACK_PKT;
// 				ack.id = 0; //this id is def not right
// 				ack.role = 0;
// 				ack.size = 0;
// 				time_t sec;
// 				time(&sec);
// 				ack.timestamp_sec = (uint32_t)sec;
// 				ack.timestamp_nsec = (uint32_t) (sec * 10e9);
// 				client_send_packet(cli, &ack, NULL);
// 			}

// 			continue;

// 		}
// 		if(logged_in && JEUX_REVOKED_PKT) {
// 			debug("revoke packet");

// 			int result = client_revoke_invitation(cli, hdr->id);
// 			if(!result){
// 				client_send_ack(cli, NULL ,0);
// 			}
// 			else{
// 				client_send_nack(cli);
// 			}
// 			// is the revoked sent by the function?
// 			continue;
// 		}

// 		if(logged_in && JEUX_DECLINE_PKT){
// 			debug("decline packet");
// 			int result = client_decline_invitation(cli, hdr->id);
// 			if(!result){
// 				client_send_ack(cli, NULL, 0);
// 			}
// 			else {
// 				client_send_nack(cli);
// 			}

// 			continue;
// 		}

// 		if(logged_in && JEUX_ACCEPT_PKT) {
// 			debug("accept packet");
// 			// int inv_id = hdr->id;

// 		}



// 	}
// }