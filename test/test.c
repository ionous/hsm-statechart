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
typedef hsm_bool (*testfn_t)();

// generic success becomes generic failure; failure is success.
#define WANT_TRUE( x )  ( (x) == HSM_TRUE )  ? 0 ? : -1;
#define WANT_FALSE( x ) ( (x) == HSM_FALSE ) ? 0 ? : -1;

       
//---------------------------------------------------------------------------
// Just one state.
//---------------------------------------------------------------------------

EMPTY_STATE( OneState, HsmTopState, 0 );

//---------------------------------------------------------------------------
hsm_bool FailSequence() 
{
    const char * NeverMatch[]= {
        "FALSE", NULL
    };
    hsm_machine_t hsm;
    return TestEventSequence( HsmMachine(&hsm), OneState(),  NeverMatch );
}    

//---------------------------------------------------------------------------
hsm_bool EmptySequence() 
{
    const char * EmptySequenceTest[]= {
        "OneState-ENTRY", 
        "j", "EVT-j", // unhandled event
        NULL
    };
    hsm_machine_t hsm;
    return TestEventSequence( HsmMachine(&hsm), OneState(),  EmptySequenceTest );
}    


//---------------------------------------------------------------------------
// Simple parent-child hierarchy
//---------------------------------------------------------------------------

EMPTY_STATE( I0, HsmTopState, I1 );
    EMPTY_STATE( I1, I0, I2 );
        EMPTY_STATE( I2, I1, 0 );    

hsm_bool InitSequence() 
{
    const char * ISeq1[] = {
        "I0-ENTRY", "I0-INIT", "I1-ENTRY", "I1-INIT", "I2-ENTRY", 
        NULL
    };
    hsm_machine_t hsm;
    return TestEventSequence( HsmMachine(&hsm), I0(),  ISeq1 );
}

//---------------------------------------------------------------------------
// Tests from other files
//---------------------------------------------------------------------------

hsm_bool SamekPlusTest();
hsm_bool SamekPlusBuilderTest();

#ifdef TEST_LUA
hsm_bool LuaTest();
#endif

//---------------------------------------------------------------------------
// Test Helper 
//---------------------------------------------------------------------------
int RunTest( const char * name, testfn_t test, hsm_bool want )
{
    hsm_bool res;
    printf( "%s \n", name );   
    res= test();  
    printf( "%s %s\n", name, res ? "passes." : "FAILS." ); 
    return res == want ? 0 : -1;
}

#define RUN_FALSE_TEST( x ) RunTest( #x, x, HSM_FALSE )
#define RUN_TEST( x ) RunTest( #x, x, HSM_TRUE  )

//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{  
    int tests=0;

    tests+= RUN_FALSE_TEST( FailSequence  );
    tests+= RUN_TEST( EmptySequence );
    tests+= RUN_TEST( InitSequence );
    tests+= RUN_TEST( SamekPlusTest );
    tests+= RUN_TEST( SamekPlusBuilderTest );
#ifdef TEST_LUA
    tests+= RUN_TEST( LuaTest );
#endif

    printf("tests have finished with %d failures\n", -tests);
    printf("press <enter> to exit...\n");
    getchar();
    return tests;
}
