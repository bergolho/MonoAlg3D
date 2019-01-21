/*
 * opts.h
 *
 *  Created on: 27/05/2011
 *      Author: sachetto
 */

#ifndef OPTS_H_
#define OPTS_H_

#include <getopt.h>

enum type {ALG, PETSC, ERROR};
enum dim {D2, D3, DERROR};

struct globalArgs_t {
    const char *inDirName;	/* -i option */
    bool adaptive;	                /*-a option */
    bool binary;                    //-b
} globalArgs;

static const char *optString = "bai:t:l:d:m:s:h?";

/* Display program usage, and exit.
 */
void display_usage( )
{
    puts( "ConvertToVTK - Convert Monodomain text output to VTK" );
    puts( "Usage: ConvertToVTK -t inputType -l sideLenght -i inDirName -m meshFile -d dimension [-a] [-s time step] [-b]");
    puts( "inDirName: Dir containing the text output" );
    puts( "a: If -a is set the mesh will be calculated in each time step." );
    puts( "b: The input is in binary format" );

    exit( EXIT_FAILURE );
}

void parseOptions(int argc, char**argv) {
    int opt = 0;

    /* Initialize globalArgs before we get to work. */
    globalArgs.inDirName = NULL;
    globalArgs.adaptive = false;
    globalArgs.binary = false;

    opt = getopt( argc, argv, optString );

    while( opt != -1 ) {
        switch( opt ) {
            case 'i':
                globalArgs.inDirName = optarg;
                break;
            case 'a':
                globalArgs.adaptive = true;
                break;
            case 'b':
                globalArgs.binary = true;
                break;
            case 'h':	/* fall-through is intentional */
            case '?':
                display_usage();
                break;

            default:
                /* You won't actually get here. */
                break;
        }

        opt = getopt( argc, argv, optString );
    }

    if(!globalArgs.inDirName) {
        display_usage();
    }


}

#endif /* OPTS_H_ */
