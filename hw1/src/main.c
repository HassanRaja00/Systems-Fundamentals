#include <stdio.h>
#include <stdlib.h>

#include "const.h"
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

int main(int argc, char **argv)
{
    if(validargs(argc, argv))
        USAGE(*argv, EXIT_FAILURE);
    if(global_options & 1)
        USAGE(*argv, EXIT_SUCCESS);
    // TO BE IMPLEMENTED
    if(global_options & 2) {
        uint32_t length = 12400;
        // FILE *f1;
        // f1 = fopen("/rsrc/dtmf_all.txt", "r");
        // FILE *f2;
        // f2 = fopen("/rsrc/test.au", "w");
    	dtmf_generate(stdin, stdout, length);


    }
    if(global_options & 4) {
        //calls the detect method
        dtmf_detect(stdin, stdout);
    }
    return EXIT_FAILURE;
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
