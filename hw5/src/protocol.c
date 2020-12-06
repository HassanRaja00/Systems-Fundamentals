#include <unistd.h>

#include "protocol.h"
#include "csapp.h"
#include "debug.h"


int proto_send_packet(int fd, JEUX_PACKET_HEADER *hdr, void *data) {
	//convert hdr from network to host-byte order
	uint16_t payload_size = ntohs(hdr->size);
	debug("### sending packet of type %d, id %d ###", hdr->type, hdr->id);
	debug("### payload size in send: %d ###", payload_size);

	if(rio_writen(fd, (void*)hdr, sizeof(JEUX_PACKET_HEADER)) < 0){
		return -1;
	}
	//now check if payload size is 0, and send payload if so
	if(payload_size != 0 && data != NULL){
		debug(" ### sending %s ###", (char*)data);
		if(rio_writen(fd, data, payload_size) < 0) {
			return -1;
		}

	}
	debug("### send successful ###");
	return 0;
}

int proto_recv_packet(int fd, JEUX_PACKET_HEADER *hdr, void **payloadp) {
	if(rio_readn(fd, (void*) hdr, sizeof(JEUX_PACKET_HEADER)) <= 0) {
		debug("### unsuccessful read header ###");
		return -1;
	}
	debug("### successful read ###");
	uint16_t payload_size = ntohs(hdr->size);
	debug("### payload size in receive: %d ###", payload_size);
	if(payload_size != 0){
		//+1 for \0
		*payloadp = malloc(payload_size+1); //free not this functions responsibility
		if(rio_readn(fd, *payloadp, payload_size) <= 0) {
			debug("### unsuccessful read payload ###");
			return -1;
		}
		char* str = (char*)*payloadp;
		str[payload_size] = '\0'; //not sure if this is correct bruh
		debug("### %s, %d ###", str, payload_size);
		debug("### payload read successfully ###");

	}
	else {
		*payloadp = NULL;
	}
	debug("### received packet of type %d, id %d ###", hdr->type, hdr->id);


	return 0;
}