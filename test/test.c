/**
 * @file main.c
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include "test.h"
#include <assert.h>
#include <stdio.h>

       
//---------------------------------------------------------------------------
EMPTY_STATE( OneState, HsmTopState, 0 );

const char * NeverMatch[]= {
    "FALSE", NULL
};

const char * EmptySequenceTest[]= {
    "OneState-ENTRY", 
    "j", "EVT-j", // unhandled event
    NULL
};

MAKE_TEST( FailSequence, HSM_FALSE, TestEventSequence, ( hsm, OneState(),  NeverMatch ) );
MAKE_TEST( EmptySequence, HSM_TRUE, TestEventSequence, ( hsm, OneState(),  EmptySequenceTest ) );

//---------------------------------------------------------------------------
EMPTY_STATE( I0, HsmTopState, I1 );
    EMPTY_STATE( I1, I0, I2 );
        EMPTY_STATE( I2, I1, 0 );    

static const char * ISeq1[] = {
    "I0-ENTRY", "I0-INIT", "I1-ENTRY", "I1-INIT", "I2-ENTRY", 
    NULL
};
MAKE_TEST( InitSequence, HSM_TRUE, TestEventSequence, ( hsm, I0(),  ISeq1 ) );

int SamekPlusTest();

//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{  
    int tests=0;
    hsm_machine_t hsm;
    tests+= FailSequence( HsmMachine( &hsm ) );
    tests+= EmptySequence( HsmMachine( &hsm ) );
    tests+= InitSequence( HsmMachine( &hsm ) );
    tests+= SamekPlusTest();
    printf("tests have finished with %d failures\n", -tests);
    printf("press <enter> to exit...\n");
    getchar();
    return tests;
}
