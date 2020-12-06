#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "const.h"
#include "audio.h"
#include "dtmf.h"
#include "dtmf_static.h"
#include "goertzel.h"
#include "debug.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

/*
 * You may modify this file and/or move the functions contained here
 * to other source files (except for main.c) as you wish.
 *
 * IMPORTANT: You MAY NOT use any array brackets (i.e. [ and ]) and
 * you MAY NOT declare any arrays or allocate any storage with malloc().
 * The purpose of this restriction is to force you to use pointers.
 * Variables to hold the pathname of the current file or directory
 * as well as other data have been pre-declared for you in const.h.
 * You must use those variables, rather than declaring your own.
 * IF YOU VIOLATE THIS RESTRICTION, YOU WILL GET A ZERO!
 */

/*
The purpose of tis function is to read a char
from a text file in order to convert it to an int
for the dtmf_generate() function
*/
int getNumFromText(FILE *events_in) {
    int result = 0;
    int c = fgetc(events_in);
    if(c == EOF){
        return -1;
    }
    while(c != '\t') {
        if(c==10)
            continue;
        result *= 10;
        c -= 48;
        result += c;
        c = fgetc(events_in);
    }
    return result;
}

char getSymbolFromText(FILE *events_in) {
    char c = fgetc(events_in);
    return c;
}

int checkSymbolIsCorrect(char c) {
    // debug("Checking if symbol is valid");
    uint8_t *validSymbols;
    validSymbols = *dtmf_symbol_names;
    int found = -1;
    for(int row = 0; row< 4; row++){
        for(int col = 0; col<4; col++){
            char valid = *(validSymbols + (row*4) + col);
            if(valid == c){
                found = 0;
                break;
            }

        }
    }
    return found;
}

int getFreqRow(char c) {
    if(c == '1' || c == '2' || c == '3' || c == 'A')
        return 697;
    else if(c == '4' || c == '5' || c == '6' || c == 'B')
        return 770;
    else if(c == '7' || c == '8' || c == '9' || c == 'C')
        return 852;
    else // if *, 0, #, D
        return 941;
}

int getFreqCol(char c) {
    if(c == '1' || c == '4' || c == '7' || c == '*')
        return 1209;
    else if(c == '2' || c == '5' || c == '8' || c == '0')
        return 1336;
    else if(c == '3' || c == '6' || c == '9' || c == '#')
        return 1477;
    else // if A, B, C, D
        return 1633;
}

/**
 * DTMF generation main function.
 * DTMF events are read (in textual tab-separated format) from the specified
 * input stream and audio data of a specified duration is written to the specified
 * output stream.  The DTMF events must be non-overlapping, in increasing order of
 * start index, and must lie completely within the specified duration.
 * The sample produced at a particular index will either be zero, if the index
 * does not lie between the start and end index of one of the DTMF events, or else
 * it will be a synthesized sample of the DTMF tone corresponding to the event in
 * which the index lies.
 *
 *  @param events_in  Stream from which to read DTMF events.
 *  @param audio_out  Stream to which to write audio header and sample data.
 *  @param length  Number of audio samples to be written.
 *  @return 0 if the header and specified number of samples are written successfully,
 *  EOF otherwise.
 bin/dtmf -g < rsrc/dtmf_all.txt > rsrc/test.au
 */
int dtmf_generate(FILE *events_in, FILE *audio_out, uint32_t length) {
    // TO BE IMPLEMENTED
    //call either read or write here
    debug("Inside dtmf_generate");
    if(events_in == NULL){
        return EXIT_FAILURE;
    }
    //????what is the .data_size field supposed to be???
    AUDIO_HEADER audio_header = (AUDIO_HEADER){.magic_number = 0x2e736e64,
        .data_offset = 24, .data_size = (2*length), .encoding = 3,
        .sample_rate = 8000, .channels = 1};
    audio_write_header(audio_out, &audio_header);
    int c = fgetc(events_in);
    int Fr, Fc;
    int sample_index = 0;
    while(c != EOF && sample_index < length) {
        int n1 = getNumFromText(events_in);
        int n2 = getNumFromText(events_in);
        char symbol = getSymbolFromText(events_in);
        //check if n1 is not greater than n2
        //and if symbol is a valid symbol
        if(n1 == -1 || n2 == -1){ // meaning that EOF has been reached
            break;
        }

        debug("n1: %d, n2: %d", n1, n2);
        if(n1 > n2) {
            debug("n1 > n2");
            return EOF;
        }
        if(checkSymbolIsCorrect(symbol) == -1){
            debug("Incorrect symbol");
            return EOF;
        }

        Fr = getFreqRow(symbol);
        Fc = getFreqCol(symbol);
        debug("%d, %d", Fr, Fc);
        while(sample_index < n1){
            audio_write_sample(audio_out, 0);
            sample_index++;
        }
        while(sample_index < n2){
            double val1 = cos(2*M_PI*Fr* sample_index / 8000);
            double val2 = cos(2*M_PI*Fc* sample_index /  8000);
            int16_t answer = (val1 + val2) * 0.5 * INT16_MAX;
            if(noise_file != NULL){
                double exp = pow(10, (noise_level/10));
                double w = exp/(exp+1);
                answer *= w;
            }
            audio_write_sample(audio_out, answer);
            sample_index++;

        }
        if(sample_index < length && noise_file != NULL){
        //write 0's
            while(sample_index < length){ //if EOF is reached before all samples, write 0s
                audio_write_sample(audio_out, 0);
                sample_index++;
            }
        }


        c = fgetc(events_in);

    }

    debug("End of generate success");
    return 0;
}

//below are the helper functions for dtmf_detect
int is_greater_than_other_frequencies(double strongest, double o1, double o2, double o3){
    // if you subtract the strongest by other freq and it is at least 6dB, return 0 (true)
    // debug("Strongest: %f, o1: %f, o2: %f, o3: %f", strongest, o1, o2, o3);
    if((strongest/o1) < SIX_DB || (strongest/o2) < SIX_DB || (strongest/o3) < SIX_DB){
        return -1;
    }
    else{
        return 0;
    }

}

int validate_strongest_frequencies(double strongest_row_freq, double strongest_col_freq, double *goertzel_strengths){

    //check if strongest row freq is at is 6dB greater than other strengths
        if(strongest_row_freq == *(goertzel_strengths + 0)){
            // compare with other freqs
            if(is_greater_than_other_frequencies(strongest_row_freq, *(goertzel_strengths+1), *(goertzel_strengths+2), *(goertzel_strengths+3)) == -1){
                return -1;
            }
        }
        else if(strongest_row_freq == *(goertzel_strengths + 1)){
            if(is_greater_than_other_frequencies(strongest_row_freq, *(goertzel_strengths+0), *(goertzel_strengths+2), *(goertzel_strengths+3)) == -1){
                return -1;
            }
        }
        else if(strongest_row_freq == *(goertzel_strengths+2)){
            if(is_greater_than_other_frequencies(strongest_row_freq, *(goertzel_strengths+1), *(goertzel_strengths+0), *(goertzel_strengths+3)) == -1){
                return -1;
            }
        }
        else{ // meaning that strongest_row_freq is r3
            if(is_greater_than_other_frequencies(strongest_row_freq, *(goertzel_strengths+0), *(goertzel_strengths+1), *(goertzel_strengths+2)) == -1){
                return -1;
            }
        }

        //check if strongest col freq is at is 6dB greater than other strengths
        if(strongest_col_freq == *(goertzel_strengths+4)){
            // compare with other freqs
            if(is_greater_than_other_frequencies(strongest_col_freq, *(goertzel_strengths+5), *(goertzel_strengths+6), *(goertzel_strengths+7)) == -1){
                return -1;
            }
        }
        else if(strongest_col_freq == *(goertzel_strengths+5)){
            if(is_greater_than_other_frequencies(strongest_col_freq, *(goertzel_strengths+4), *(goertzel_strengths+6), *(goertzel_strengths+7)) == -1){
                return -1;
            }
        }
        else if(strongest_col_freq == *(goertzel_strengths+6)){
            if(is_greater_than_other_frequencies(strongest_col_freq, *(goertzel_strengths+4), *(goertzel_strengths+5), *(goertzel_strengths+7)) == -1){
                return -1;
            }
        }
        else{ // meaning that strongest_col_freq is r7
            if(is_greater_than_other_frequencies(strongest_col_freq, *(goertzel_strengths+4), *(goertzel_strengths+5), *(goertzel_strengths+6)) == -1){
                return -1;
            }
        }

        return 0;
}

char findSymbolFromFrequencies(int row_freq, int col_freq){
    if(row_freq == 697){
        if(col_freq == 1209)
            return '1';

        if(col_freq == 1336)
            return '2';

        if(col_freq == 1477)
            return '3';

        if(col_freq == 1633)
            return 'A';
    }
    else if(row_freq == 770){
        if(col_freq == 1209)
            return '4';

        if(col_freq == 1336)
            return '5';

        if(col_freq == 1477)
            return '6';

        if(col_freq == 1633)
            return 'B';
    }
    else if(row_freq == 852){
        if(col_freq == 1209)
            return '7';

        if(col_freq == 1336)
            return '8';

        if(col_freq == 1477)
            return '9';

        if(col_freq == 1633)
            return 'C';
    }
    else{ //meaning row == 941
        if(col_freq == 1209){
            return '*';
        }

        if(col_freq == 1336){
            return '0';
        }

        if(col_freq == 1477){
            return '#';
        }

        if(col_freq == 1633){
            return 'D';
        }
    }
    // debug("row = %d, col = %d", row_freq, col_freq);
    return 'F';
}

void write_to_detect_output(FILE *events_out, int start_index, int end_index, int row_freq, int col_freq){
    fprintf(events_out, "%d", start_index);
    fprintf(events_out, "%c", '\t');
    fprintf(events_out, "%d", end_index);
    fprintf(events_out, "%c", '\t');
    char symbol = findSymbolFromFrequencies(row_freq, col_freq);
    if(symbol != EOF){
        fprintf(events_out, "%c", symbol);
    }
    fprintf(events_out, "%c", '\n'); // write a newline char afterwards

}


/**
 * DTMF detection main function.
 * This function first reads and validates an audio header from the specified input stream.
 * The value in the data size field of the header is ignored, as is any annotation data that
 * might occur after the header.
 *
 * This function then reads audio sample data from the input stream, partititions the audio samples
 * into successive blocks of block_size samples, and for each block determines whether or not
 * a DTMF tone is present in that block.  When a DTMF tone is detected in a block, the starting index
 * of that block is recorded as the beginning of a "DTMF event".  As long as the same DTMF tone is
 * present in subsequent blocks, the duration of the current DTMF event is extended.  As soon as a
 * block is encountered in which the same DTMF tone is not present, either because no DTMF tone is
 * present in that block or a different tone is present, then the starting index of that block
 * is recorded as the ending index of the current DTMF event.  If the duration of the now-completed
 * DTMF event is greater than or equal to MIN_DTMF_DURATION, then a line of text representing
 * this DTMF event in tab-separated format is emitted to the output stream. If the duration of the
 * DTMF event is less that MIN_DTMF_DURATION, then the event is discarded and nothing is emitted
 * to the output stream.  When the end of audio input is reached, then the total number of samples
 * read is used as the ending index of any current DTMF event and this final event is emitted
 * if its length is at least MIN_DTMF_DURATION.
 *
 *   @param audio_in  Input stream from which to read audio header and sample data.
 *   @param events_out  Output stream to which DTMF events are to be written.
 *   @return 0  If reading of audio and writing of DTMF events is sucessful, EOF otherwise.
 bin/dtmf -d < rsrc/dtmf_0_500ms.au > rsrc/detect_test.txt
 bin/dtmf -d < rsrc/dtmf_all.au > rsrc/detect_test.txt
 */
int dtmf_detect(FILE *audio_in, FILE *events_out) {
    // TO BE IMPLEMENTED
    debug("Inside dtmf_detect()");
    //first read in and validate audio header from input
    AUDIO_HEADER header;
    if(audio_read_header(audio_in, &header) == EOF){
        return EOF;
    }



    int16_t sample;
    int found_tone = 1;
    int prev_row = -1, prev_col = -1, curr_row, curr_col;
    int start_index = 0, end_index = 0;
    int end_file = 1;
    double duration;

    while(end_file != EOF){

        //row frequencies
        goertzel_init((goertzel_state + 0), block_size, (double) *(dtmf_freqs+ 0) * block_size / 8000);
        goertzel_init((goertzel_state + 1), block_size, (double) *(dtmf_freqs+ 1) * block_size / 8000);
        goertzel_init((goertzel_state + 2), block_size, (double) *(dtmf_freqs+ 2) * block_size / 8000);
        goertzel_init((goertzel_state + 3), block_size, (double) *(dtmf_freqs+ 3) * block_size / 8000);

        //column frequencies
        goertzel_init((goertzel_state + 4), block_size, (double) *(dtmf_freqs+ 4) * block_size / 8000);
        goertzel_init((goertzel_state + 5), block_size, (double) *(dtmf_freqs+ 5) * block_size / 8000);
        goertzel_init((goertzel_state + 6), block_size, (double) *(dtmf_freqs+ 6) * block_size / 8000);
        goertzel_init((goertzel_state + 7), block_size, (double) *(dtmf_freqs+ 7) * block_size / 8000);

        double strongest_row_strength, strongest_col_strength;
        double converted_sample;
        for(int i = 0; i < block_size - 1; i++){
            end_file = audio_read_sample(audio_in, &sample);
            if(end_file == EOF){
                break;
            }
            converted_sample = (double) sample / INT16_MAX;
            //step the rows (0-3)
            goertzel_step((goertzel_state + 0), converted_sample);
            goertzel_step((goertzel_state + 1), converted_sample);
            goertzel_step((goertzel_state + 2), converted_sample);
            goertzel_step((goertzel_state + 3), converted_sample);
            //step the columns (4-7)
            goertzel_step((goertzel_state + 4), converted_sample);
            goertzel_step((goertzel_state + 5), converted_sample);
            goertzel_step((goertzel_state + 6), converted_sample);
            goertzel_step((goertzel_state + 7), converted_sample);

            //increase the end index every time you read a sample
            end_index++;
        }
        //now find strength with last sample in block size
        if(end_file == EOF){
            break;
        }
        end_file = audio_read_sample(audio_in, &sample);

        converted_sample = (double) sample / INT16_MAX;
        end_index++;

        //find strongest row
        *(goertzel_strengths + 0) = goertzel_strength((goertzel_state + 0), converted_sample);
        curr_row = 697;
        strongest_row_strength = *(goertzel_strengths + 0);

        *(goertzel_strengths + 1) = goertzel_strength((goertzel_state + 1), converted_sample);
        if(*(goertzel_strengths + 1) > strongest_row_strength){
            strongest_row_strength = *(goertzel_strengths + 1);
            curr_row = 770;
        }

        *(goertzel_strengths + 2) = goertzel_strength((goertzel_state + 2), converted_sample);
        if(*(goertzel_strengths + 2) > strongest_row_strength){
            strongest_row_strength = *(goertzel_strengths + 2);
            curr_row = 852;
        }

        *(goertzel_strengths + 3) = goertzel_strength((goertzel_state + 3), converted_sample);
        if(*(goertzel_strengths + 3) > strongest_row_strength){
            strongest_row_strength = *(goertzel_strengths + 3);
            curr_row = 941;
        }

        //find strongest column
        *(goertzel_strengths + 4) = goertzel_strength((goertzel_state + 4), converted_sample);
        curr_col = 1209;
        strongest_col_strength = *(goertzel_strengths + 4);

        *(goertzel_strengths + 5) = goertzel_strength((goertzel_state + 5), converted_sample);
        if(*(goertzel_strengths + 5) > strongest_col_strength){
            strongest_col_strength = *(goertzel_strengths + 5);
            curr_col = 1336;
        }

        *(goertzel_strengths + 6) = goertzel_strength((goertzel_state + 6), converted_sample);
        if(*(goertzel_strengths + 6) > strongest_col_strength){
            strongest_col_strength = *(goertzel_strengths + 6);
            curr_col = 1477;
        }

        *(goertzel_strengths + 7) = goertzel_strength((goertzel_state + 7), converted_sample);
        if(*(goertzel_strengths + 7) > strongest_col_strength){
            strongest_col_strength = *(goertzel_strengths + 7);
            curr_col = 1633;
        }

        if(prev_col == -1 && prev_row == -1){
            prev_col = curr_col;
            prev_row = curr_row;
        }

        double sum_of_strengths = strongest_col_strength + strongest_row_strength;
        double ratio_of_strengths = strongest_row_strength / strongest_col_strength;
        duration = (double)(end_index - start_index) / 8000;
        debug("start_index: %d, end_index: %d", start_index, end_index);

        if(sum_of_strengths < MINUS_20DB){
            debug("Sum of strengths is too small: %f", sum_of_strengths);
            found_tone = -1;
        }
        else if(ratio_of_strengths < (1 / FOUR_DB) || ratio_of_strengths > FOUR_DB){
            debug("Ratio out of range: %f", ratio_of_strengths);
            found_tone = -1;
        }
        else if(validate_strongest_frequencies(strongest_row_strength, strongest_col_strength, goertzel_strengths) == -1){
            debug("failed validate_strongest_frequencies()");
            found_tone = -1;
        }

        if(found_tone == -1){ //a check failed, no DTMF tone
            debug("no tone found");
            if(duration >= MIN_DTMF_DURATION ){
                write_to_detect_output(events_out, start_index, (end_index - block_size), prev_row, prev_col);
            }
            start_index = end_index ;


        }
        else if(prev_row != curr_row && prev_col != curr_col){ // different DTMF tone
            debug("different tone found");
            if(duration >= MIN_DTMF_DURATION ){
                write_to_detect_output(events_out, start_index, (end_index - block_size), prev_row, prev_col);
            }
            start_index = end_index - block_size;

        }
        prev_row = curr_row;
        prev_col = curr_col;
        //keept going until EOF
        found_tone = 1;//reset flag

        // debug("strongest_row_strength = %f\n strongest_col_strength = %f\n", strongest_row_strength, strongest_col_strength);

    } // end of while loop
    //at end write total number of samples written
    // debug("%", end_index, start_index);
    if(duration >= MIN_DTMF_DURATION ){
        write_to_detect_output(events_out, start_index, end_index, curr_row, curr_col);
    }


    return 0;
    //cat dtmf_in.txt | bin/dtmf -g | bin/dtmf -d | > dtmf_out.txt
}

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 0 if validation succeeds and -1 if validation fails.
 * Upon successful return, the operation mode of the program (help, generate,
 * or detect) will be recorded in the global variable `global_options`,
 * where it will be accessible elsewhere in the program.
 * Global variables `audio_samples`, `noise file`, `noise_level`, and `block_size`
 * will also be set, either to values derived from specified `-t`, `-n`, `-l` and `-b`
 * options, or else to their default values.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 0 if validation succeeds and -1 if validation fails.
 * @modifies global variable "global_options" to contain a bitmap representing
 * the selected program operation mode, and global variables `audio_samples`,
 * `noise file`, `noise_level`, and `block_size` to contain values derived from
 * other option settings.
 */

int validargs(int argc, char **argv) //argv is basically a pointer to an array of pointers
{
    // TO BE IMPLEMENTED
    if(argc == 1)
    	return -1;
    //check the 2nd element in argv for -h, -g, -d
    char* char1 = *(argv+1); //points to the beginning of the string of 2nd string
    char* char2 = char1+1; //points to the second char of the 2nd string
    char* char3 = char1+2;
    if(*char1 == '-' && *char2 == 'h' && *char3 == '\0'){ //check if 2nd string is only -h with \0 marking end of the string
    	debug("WE FOUND -H\n");
    	// now that we have -h, set lsb of global_options to 1
    	global_options = global_options | 1; //binary is 1

    	//return success
    	return 0;
    }
    else if(*char1 == '-' && *char2 == 'g' && *char3 == '\0'){
    	debug("WE FOUND -g!\n");

    	//now check for other optional parameters
    	// after -g, there could be -t, -n, -l, BUT NOT anything else

    	if(argc == 2) { //no optional parameters given
    		//set the default values for optional params
    		audio_samples = 1000 * 8;
    		//noise file not needed I think, so set to NULL
            noise_file = NULL;
    		noise_level = 0;
    		debug("All default values set");

    	}
    	else if(argc == 4) {
    		int t = 0; //to check if we previously used this
    		int n = 0;
    		int l = 0;
    		//meaning the user provided 1 optional param
    		char1 = *(argv+2); //check if valid flag given
    		char2 = char1+1;
    		char3 = char1+2;
    		if(*char1 == '-' && *char2 == 't' && *char3 == '\0'){
    			t = 1;
    			//set the time in msec if valid
    			char1 = *(argv+3); //4th parameter
    			if(*char1 < '0' || *char1 > '9'){
    				debug("INVALID TIME NEGATIVE\n");
    				return -1; //if the input is not a digit
    			}
    			audio_samples = *char1-48;
    			char1 += 1;
    			while(*char1 != '\0') { //convert string to int
    				if(*char1 < '0' || *char1 > '9')
    					return -1; // if the converted char's value is not 0-9
    				audio_samples = audio_samples*10;
    				audio_samples += *char1-48;
    				char1 += 1;
    			}
    			debug("The MSEC you entered: %d\n", audio_samples);
    			if(audio_samples < 0){ //if the num is negative, not in range
    				debug("Invalid number for time!\n");
    				return -1;
    			}
    			audio_samples = audio_samples * 8;

    		}
    		else if(*char1 == '-' && *char2 == 'n' && *char3 == '\0'){
    			n = 1;
    			//set the noise file to whatever input is
    			noise_file = *(argv+3); //NOT SURE TO CHECK FOR EXISTENCE
    		}
    		else if(*char1 == '-' && *char2 == 'l' && *char3 == '\0'){
    			l = 1;
    			//set the noise level
    			char1 = *(argv+3); //the noise level arg
    			if(*char1 < '0' || *char1 > '9'){
    				if(*char1 == '-'){ //if this symbol, means negative number was inputted
    					char2 = char1;
    					char1 += 1;
    				}
    				else{
    					return -1; //invalid input (NaN)
    				}
    			}
    			noise_level = *char1-48;
    			char1 += 1;
    			while(*char1 != '\0') { //convert string to int
    				if(*char1 < '0' || *char1 > '9')
    					return -1; // if the converted char's value is not 0-9
    				noise_level = noise_level*10;
    				noise_level += *char1-48;
    				char1 += 1;
    			}
    			if(*char2 == '-'){ //if neg num entered
    				noise_level = noise_level * -1;
    			}
    			if(noise_level < -30 || noise_level > 30){
    				debug("INVALID NOISE LEVEL: %d\n", noise_level);
    				return -1;
    			}
    			debug("THe noise level entered was: %d\n", noise_level);
    		}
    		else{
    			return -1; //invalid flag given
    		}
    		if(t == 0){ //if t was not the flag entered
    			audio_samples = 1000 * 8;
    			debug("Default value entered for -t\n");
    		}
    		if(n == 0){
    			noise_file = NULL;
    		}
    		if(l == 0) {
    			noise_level = 0;
    			debug("Default value entered for -l\n");
    		}
    	}
    	else if(argc == 6) {
    		//meaning the user provided 2 optional params
    		int t = 0; //flags we will use to see which
    		int n = 0; //optional params were used
    		int l = 0;
    		char1 = *(argv+2); //check if valid flag given after -g
    		char2 = char1+1;
    		char3 = char1+2;
    		if(*char1 == '-' && *char2 == 't' && *char3 == '\0'){
    			t = 1; //next time we will know to not accept -t again
    			//set the time in msec if valid
    			char1 = *(argv+3); //4th parameter
    			if(*char1 < '0' || *char1 > '9'){
    				debug("INVALID TIME NEGATIVE (6)\n");
    				return -1; //if the input is not a digit
    			}
    			audio_samples = *char1-48;
    			char1 += 1;
    			while(*char1 != '\0') { //convert string to int
    				if(*char1 < '0' || *char1 > '9')
    					return -1; // if the converted char's value is not 0-9
    				audio_samples = audio_samples*10;
    				audio_samples += *char1-48;
    				char1 += 1;
    			}
    			debug("(6) The MSEC you entered: %d\n", audio_samples);
    			if(audio_samples < 0){ //if the num is negative, not in range
    				debug("(6)Invalid number for time!\n");
    				return -1;
    			}
    			audio_samples = audio_samples * 8;

    		}
    		else if(*char1 == '-' && *char2 == 'n' && *char3 == '\0'){
    			n = 1; //-n wont be accepted at the next flag
    			//set the noise file to whatever input is
    			noise_file = *(argv+3); //NOT SURE TO CHECK FOR EXISTENCE
    		}
    		else if(*char1 == '-' && *char2 == 'l' && *char3 == '\0'){
    			l = 1;
    			//set the noise level
    			char1 = *(argv+3); //the noise level arg
    			if(*char1 < '0' || *char1 > '9'){
    				if(*char1 == '-'){ //if this symbol, means negative number was inputted
    					char2 = char1;
    					char1 += 1;
    				}
    				else{
    					return -1; //invalid input (NaN)
    				}
    			}
    			noise_level = *char1-48;
    			char1 += 1;
    			while(*char1 != '\0') { //convert string to int
    				if(*char1 < '0' || *char1 > '9')
    					return -1; // if the converted char's value is not 0-9
    				noise_level = noise_level*10;
    				noise_level += *char1-48;
    				char1 += 1;
    			}
    			if(*char2 == '-'){ //if neg num entered
    				noise_level = noise_level * -1;
    			}
    			if(noise_level < -30 || noise_level > 30){
    				debug("(6) INVALID NOISE LEVEL: %d\n", noise_level);
    				return -1;
    			}
    			debug("(6) THe noise level entered was: %d\n", noise_level);
    		}
    		else{
    			return -1; //invalid flag given
    		}

    		char1 = *(argv+4); //5th parameter (-t,-n-l)
    		char2 = char1+1;
    		char3 = char1+2;
    		if(*char1 == '-' && *char2 == 't' && *char3 == '\0'){
    			if(t==1){
    				debug("This flag was already used!");
    				return -1;
    			}
    			t = 1; //next time we will know to not accept -t again
    			//set the time in msec if valid
    			char1 = *(argv+5); //6th parameter
    			if(*char1 < '0' || *char1 > '9'){
    				debug("INVALID TIME NEGATIVE (6-2)\n");
    				return -1; //if the input is not a digit
    			}
    			audio_samples = *char1-48;
    			char1 += 1;
    			while(*char1 != '\0') { //convert string to int
    				if(*char1 < '0' || *char1 > '9')
    					return -1; // if the converted char's value is not 0-9
    				audio_samples = audio_samples*10;
    				audio_samples += *char1-48;
    				char1 += 1;
    			}
    			debug("(6-2) The MSEC you entered: %d\n", audio_samples);
    			if(audio_samples < 0){ //if the num is negative, not in range
    				debug("(6-2)Invalid number for time!\n");
    				return -1;
    			}
    			audio_samples = audio_samples * 8;
    		}
    		else if(*char1 == '-' && *char2 == 'n' && *char3 == '\0'){
    			if(n==1){
    				debug("This flag was already used!");
    				return -1;
    			}
    			n = 1; //-n wont be accepted at the next flag
    			//set the noise file to whatever input is
    			noise_file = *(argv+5); //NOT SURE TO CHECK FOR EXISTENCE
    		}
    		else if(*char1 == '-' && *char2 == 'l' && *char3 == '\0'){
    			if(l==1){
    				debug("This flag was already used!\n");
    				return -1;
    			}
    			l = 1;
    			//set the noise level
    			char1 = *(argv+5); //the noise level arg
    			if(*char1 < '0' || *char1 > '9'){
    				if(*char1 == '-'){ //if this symbol, means negative number was inputted
    					char2 = char1;
    					char1 += 1;
    				}
    				else{
    					return -1; //invalid input (NaN)
    				}
    			}
    			noise_level = *char1-48;
    			char1 += 1;
    			while(*char1 != '\0') { //convert string to int
    				if(*char1 < '0' || *char1 > '9')
    					return -1; // if the converted char's value is not 0-9
    				noise_level = noise_level*10;
    				noise_level += *char1-48;
    				char1 += 1;
    			}
    			if(*char2 == '-'){ //if neg num entered
    				noise_level = noise_level * -1;
    			}
    			if(noise_level < -30 || noise_level > 30){
    				debug("(6-2) INVALID NOISE LEVEL: %d\n", noise_level);
    				return -1;
    			}
    			debug("(6-2) THe noise level entered was: %d\n", noise_level);
    		}
    		else{
    			return -1; //invalid flag given
    		}

    		//set default val for unset flags
    		if(t == 0){
    			audio_samples = 1000 * 8;
    			debug("Default value entered for -t\n");
    		}
    		if(n == 0){
    		  noise_file = NULL;
    		}
    		if(l==0){
    			noise_level = 0;
    			debug("Default value entered for -l\n");
    		}

    	}
    	else if(argc == 8) {
    		//meaning the user provided all optional params
    		int t = 0; //flags we will use to see which
    		int n = 0; //optional params were used
    		int l = 0;
    		char1 = *(argv+2); //check if valid flag given after -g
    		char2 = char1+1;
    		char3 = char1+2;
    		if(*char1 == '-' && *char2 == 't' && *char3 == '\0'){
    			t = 1; //if t is found again, error
    			char1 = *(argv+3); //4th parameter
    			if(*char1 < '0' || *char1 > '9'){
    				debug("INVALID TIME NEGATIVE (8-1)\n");
    				return -1; //if the input is not a digit
    			}
    			audio_samples = *char1-48;
    			char1 += 1;
    			while(*char1 != '\0') { //convert string to int
    				if(*char1 < '0' || *char1 > '9')
    					return -1; // if the converted char's value is not 0-9
    				audio_samples = audio_samples*10;
    				audio_samples += *char1-48;
    				char1 += 1;
    			}
    			debug("(8-1) The MSEC you entered: %d\n", audio_samples);
    			if(audio_samples < 0){ //if the num is negative, not in range
    				debug("(8-1)Invalid number for time!\n");
    				return -1;
    			}
    			audio_samples = audio_samples * 8;
    		}
    		else if(*char1 == '-' && *char2 == 'n' && *char3 == '\0'){
    			n = 1;
    			//set the noise file to whatever input is
    			noise_file = *(argv+3); //NOT SURE TO CHECK FOR EXISTENCE
    		}
    		else if(*char1 == '-' && *char2 == 'l' && *char3 == '\0'){
    			l = 1;
    			//set the noise level
    			char1 = *(argv+3); //the noise level arg
    			if(*char1 < '0' || *char1 > '9'){
    				if(*char1 == '-'){ //if this symbol, means negative number was inputted
    					char2 = char1;
    					char1 += 1;
    				}
    				else{
    					return -1; //invalid input (NaN)
    				}
    			}
    			noise_level = *char1-48;
    			char1 += 1;
    			while(*char1 != '\0') { //convert string to int
    				if(*char1 < '0' || *char1 > '9')
    					return -1; // if the converted char's value is not 0-9
    				noise_level = noise_level*10;
    				noise_level += *char1-48;
    				char1 += 1;
    			}
    			if(*char2 == '-'){ //if neg num entered
    				noise_level = noise_level * -1;
    			}
    			if(noise_level < -30 || noise_level > 30){
    				debug("(8-1) INVALID NOISE LEVEL: %d\n", noise_level);
    				return -1;
    			}
    			debug("(8-1) THe noise level entered was: %d\n", noise_level);

    		}
    		else {
    			return -1; //invalid flag
    		}

    		char1 = *(argv+4); //5th parameter (-t,-n-l)
    		char2 = char1+1;
    		char3 = char1+2;
    		if(*char1 == '-' && *char2 == 't' && *char3 == '\0'){
    			if(t==1){
    				debug("This flag was already used!");
    				return -1;
    			}
    			t = 1; //next time we will know to not accept -t again
    			//set the time in msec if valid
    			char1 = *(argv+5); //6th parameter
    			if(*char1 < '0' || *char1 > '9'){
    				debug("INVALID TIME NEGATIVE (8-2)\n");
    				return -1; //if the input is not a digit
    			}
    			audio_samples = *char1-48;
    			char1 += 1;
    			while(*char1 != '\0') { //convert string to int
    				if(*char1 < '0' || *char1 > '9')
    					return -1; // if the converted char's value is not 0-9
    				audio_samples = audio_samples*10;
    				audio_samples += *char1-48;
    				char1 += 1;
    			}
    			debug("(8-2) The MSEC you entered: %d\n", audio_samples);
    			if(audio_samples < 0){ //if the num is negative, not in range
    				debug("(8-2)Invalid number for time!\n");
    				return -1;
    			}
    			audio_samples = audio_samples * 8;

    		}
    		else if(*char1 == '-' && *char2 == 'n' && *char3 == '\0'){
    			if(n==1){
    				debug("This flag was already used!");
    				return -1;
    			}
    			n = 1; //-n wont be accepted at the next flag
    			//set the noise file to whatever input is
    			noise_file = *(argv+5); //NOT SURE TO CHECK FOR EXISTENCE
    		}
    		else if(*char1 == '-' && *char2 == 'l' && *char3 == '\0'){
    			if(l==1){
    				debug("This flag was already used!\n");
    				return -1;
    			}
    			l = 1;
    			//set the noise level
    			char1 = *(argv+5); //the noise level arg
    			if(*char1 < '0' || *char1 > '9'){
    				if(*char1 == '-'){ //if this symbol, means negative number was inputted
    					char2 = char1;
    					char1 += 1;
    				}
    				else{
    					return -1; //invalid input (NaN)
    				}
    			}
    			noise_level = *char1-48;
    			char1 += 1;
    			while(*char1 != '\0') { //convert string to int
    				if(*char1 < '0' || *char1 > '9')
    					return -1; // if the converted char's value is not 0-9
    				noise_level = noise_level*10;
    				noise_level += *char1-48;
    				char1 += 1;
    			}
    			if(*char2 == '-'){ //if neg num entered
    				noise_level = noise_level * -1;
    			}
    			if(noise_level < -30 || noise_level > 30){
    				debug("(8-2) INVALID NOISE LEVEL: %d\n", noise_level);
    				return -1;
    			}
    			debug("(8-2) THe noise level entered was: %d\n", noise_level);
    		}
    		else{
    			return -1; //invalid flag given
    		}
    		//now check last flag (should be unused)
    		char1 = *(argv+6); //7th parameter (-t,-n-l)
    		char2 = char1+1;
    		char3 = char1+2;
    		if(*char1 == '-' && *char2 == 't' && *char3 == '\0'){
    			if(t==1){
    				debug("This flag was already used!");
    				return -1;
    			}
    			t = 1; //next time we will know to not accept -t again
    			//set the time in msec if valid
    			char1 = *(argv+7); //6th parameter
    			if(*char1 < '0' || *char1 > '9'){
    				debug("INVALID TIME NEGATIVE (8-3)\n");
    				return -1; //if the input is not a digit
    			}
    			audio_samples = *char1-48;
    			char1 += 1;
    			while(*char1 != '\0') { //convert string to int
    				if(*char1 < '0' || *char1 > '9')
    					return -1; // if the converted char's value is not 0-9
    				audio_samples = audio_samples*10;
    				audio_samples += *char1-48;
    				char1 += 1;
    			}
    			debug("(8-3) The MSEC you entered: %d\n", audio_samples);
    			if(audio_samples < 0){ //if the num is negative, not in range
    				debug("(8-3)Invalid number for time!\n");
    				return -1;
    			}
    			audio_samples = audio_samples * 8;

    		}
    		else if(*char1 == '-' && *char2 == 'n' && *char3 == '\0'){
    			if(n==1){
    				debug("This flag was already used!");
    				return -1;
    			}
    			n = 1; //-n wont be accepted at the next flag
    			//set the noise file to whatever input is
    			noise_file = *(argv+7); //NOT SURE TO CHECK FOR EXISTENCE
    		}
    		else if(*char1 == '-' && *char2 == 'l' && *char3 == '\0'){
    			if(l==1){
    				debug("This flag was already used!\n");
    				return -1;
    			}
    			l = 1;
    			//set the noise level
    			char1 = *(argv+7); //the noise level arg
    			if(*char1 < '0' || *char1 > '9'){
    				if(*char1 == '-'){ //if this symbol, means negative number was inputted
    					char2 = char1;
    					char1 += 1;
    				}
    				else{
    					return -1; //invalid input (NaN)
    				}
    			}
    			noise_level = *char1-48;
    			char1 += 1;
    			while(*char1 != '\0') { //convert string to int
    				if(*char1 < '0' || *char1 > '9')
    					return -1; // if the converted char's value is not 0-9
    				noise_level = noise_level*10;
    				noise_level += *char1-48;
    				char1 += 1;
    			}
    			if(*char2 == '-'){ //if neg num entered
    				noise_level = noise_level * -1;
    			}
    			if(noise_level < -30 || noise_level > 30){
    				debug("(8-3) INVALID NOISE LEVEL: %d\n", noise_level);
    				return -1;
    			}
    			debug("(8-3) THe noise level entered was: %d\n", noise_level);
    		}
    		else{
    			return -1; //invalid flag given
    		}

    	}
    	else {
    		return -1; //invalid amount of flags
    	}
    	//set second to lsb to 1 of global_options
    	global_options = global_options | 2; //binary is 10
    	debug("Global options after -g: %d\n", global_options);

    	return 0; //return success UNCOMMENT WHEN COMPLETE
    }
    else if(*char1 == '-' && *char2 == 'd' && *char3 == '\0'){
    	debug("WE FOUND -d\n");

    	//check for optional param
    	if(argc == 2) {
    		//set the default block size
    		block_size = 100;
    	}
    	else if(argc == 4) {
    		//validate that next string is -b, else fail
    		char1 = *(argv+2); //3rd string in array
    		char2 = char1 + 1;
    		char3 = char1 + 2;
    		if(*char1 == '-' && *char2 == 'b' && *char3 == '\0') {
    			//now check for a valid number after parsing
    			char1 = *(argv+3); // 4th string in argv
    			if(*char1 < '0' || *char1 > '9')
    				return -1; // if the converted char's value is not 0-9
    			block_size = *char1-48; //set ms digit
    			char1 = char1 + 1;
    			while(*char1 != '\0') {
    				if(*char1 < '0' || *char1 > '9')
    					return -1; // if the converted char's value is not 0-9
    				block_size = block_size*10;
    				block_size += *char1-48;
    				char1 += 1;

    			}
    			debug("block size: %d\n", block_size);
    			if(block_size >= 10 && block_size <= 1000) {
    				debug("Valid value for block size!\n");
    			} else {
    				debug("Invalid value for block size\n");
    				return -1;
    			}
    		}
    		else {
    			return -1; // invalid parameter name
    		}
    		//set the input if within range
    	}
    	else {
    		debug("Nothing after -b!\n");
    		return -1; //invalid input
    	}
    	//set third to lsb to 1 of global_options
    	global_options = global_options | 4; //binary is 100

    	return 0;
    }
    else {
    	debug("WE GOT NOTHING\n");
    	return -1;
    }
    debug("Global options is: %d\n", global_options);
    return -1;
}
