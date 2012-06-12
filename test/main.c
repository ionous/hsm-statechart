/**
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * All code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include <stdio.h>

typedef int(*test_function)( int argc, char* argv[] );


int watch1_named_events( int argc, char* argv[] );
int watch1_enum_events( int argc, char* argv[] );

int main(int argc, char* argv[])
{   
    test_function tests[]= {
        watch1_named_events,
        watch1_enum_events
    };
    int test=0;

    if (argc > 1) {
        sscanf( argv[1], "%d", &test );
    }

    if (test >= sizeof(tests)/sizeof(test_function)) {
        printf("unknown test");
    }
    else {
        tests[test]( argc-1, argv+1 );
    }
}
