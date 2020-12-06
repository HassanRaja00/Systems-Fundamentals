#include <stdio.h>

#include "audio.h"
#include "debug.h"

int audio_read_header(FILE *in, AUDIO_HEADER *hp) {
    // TO BE IMPLEMENTED
    if(in == NULL) {
    	debug("File is unknown/NULL\n");
    	return EOF;
    }
    uint32_t magic_num;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t encoding;
    uint32_t sample_rate;
    uint32_t channels;
    int c = 0;
    for(int i = 1; i < 25; i++) {
    	c = c << 8; //to make room for next hex num
    	int val = fgetc(in);
    	if(val == EOF)
    		return EOF;
    	c += val;
    	if(i%4 == 0){
    		if(i == 4)
    			magic_num = c;
    		if(i==8)
    			data_offset = c;
    		if(i==12)
    			data_size = c;
    		if(i==16)
    			encoding = c;
    		if(i==20)
    			sample_rate = c;
    		if(i==24)
    			channels = c;
    		c = 0;
    	}

    }
    if(magic_num != 0x2e736e64)
    	return EOF; //not sure what to do with this if its not
    if(data_offset < 24)
    	return EOF;
    if(data_size == 0xffffffff)
    	return EOF;
    if(encoding != 3)
    	return EOF;
    if(sample_rate != 8000)
    	return EOF;
    if(channels != 1)
    	return EOF;

    //now set the struct values to these!
    // debug("All values pass criteria for hw\n");
    AUDIO_HEADER audio_header;
    audio_header = (AUDIO_HEADER){.magic_number = magic_num,
    	.data_offset = data_offset, .data_size = data_size,
    	.encoding = encoding, .sample_rate = sample_rate,
    	.channels = channels};

    *hp = audio_header; //set the struct pointer to this struct


    //now you need to continue looking at every char until you get to '\0'
    // do{
    // 	c = fgetc(in); //75fa is the first frame
    // } while(c != '\0');

    return 0;
}

void write_bytes(uint32_t num, FILE *out) {

	for(int i = 3; i>=0; i--) {
    	int x = (num >> (8*i)) & 0xff;
    	fputc(x, out);

    }
}

int audio_write_header(FILE *out, AUDIO_HEADER *hp) {
    // TO BE IMPLEMENTED

    AUDIO_HEADER audio_header = *hp;
    // debug("INSIDE audio_write_header()");
    if(audio_header.magic_number != 0x2e736e64){
    	return EOF;
    }
    if(audio_header.data_offset < 24){
    	return EOF;
    }
    if(audio_header.data_size == 0xffffffff){
    	return EOF;
    }
    if(audio_header.encoding != 3){
    	return EOF;
    }
    if(audio_header.sample_rate != 8000){
    	return EOF;
    }
    if(audio_header.channels != 1){
    	return EOF;
    }
	write_bytes(audio_header.magic_number, out);
	write_bytes(audio_header.data_offset, out);
	write_bytes(audio_header.data_size, out);
	write_bytes(audio_header.encoding, out);
	write_bytes(audio_header.sample_rate, out);
	write_bytes(audio_header.channels, out);

	// for(int i = 0; i < 8; i++){
	// 	fputc(2, out);
	// }
	// fputc('\0', out);// 0 is a multiple of 8 so we put '\0'  after 8 random vals

    return 0;
}

int audio_read_sample(FILE *in, int16_t *samplep) {
    // TO BE IMPLEMENTED
    // debug("INSIDE audio_read_sample");
    if(in == NULL)
    	return EOF;

    int16_t val = 0;
    val = fgetc(in);
    if(val == EOF)
    	return EOF;

    val = val << 8;
    val += fgetc(in);

    *samplep = val;

    return 0;
}

int audio_write_sample(FILE *out, int16_t sample) {
    // TO BE IMPLEMENTED
    if(out == NULL )
    	return EOF;
    // debug("inside audio_write_sample()");
    int16_t byte1 = sample >> 8;
    fputc(byte1, out);
    fputc(sample, out); //this will add the second byte

    return 0;
}
