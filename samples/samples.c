/**
 * @file samples.c
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include <stdio.h>

typedef int(*samples_function)( int argc, char* argv[] );

int watch1_named_events( int argc, char* argv[] );
int watch1_enum_events( int argc, char* argv[] );
int watch_builder(int argc, char* argv[] );
int watch_lua(int argc, char* argv[] );

int main(int argc, char* argv[])
{   
    samples_function samples[]= {
#ifdef TEST_LUA
        watch_lua,
#endif
        watch_builder,
        watch1_named_events,
        watch1_enum_events,
    };
    int which=0;

    if (argc > 1) {
        sscanf( argv[1], "%d", &which );
    }

    if (which >= sizeof(samples)/sizeof(samples_function)) {
        printf("unknown sample selected.");
    }
    else {
        samples[which]( argc-1, argv+1 );
    }
}
