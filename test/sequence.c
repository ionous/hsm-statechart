/**
 * @file sequence.c
 *
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include "test.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

//---------------------------------------------------------------------------
typedef struct test_sequence_rec test_sequence_t;
struct test_sequence_rec {
    const char ** sequence;
    int index;
    hsm_bool error;
};

//---------------------------------------------------------------------------
// on success: advances to the next test, on failure: flags 
static hsm_bool Verify( test_sequence_t * test, const char * string, ... )
{
    hsm_bool okay= HSM_FALSE;
    if (!test->error) {
        va_list args;
        char result[256];
        const char * want= test->sequence[test->index];

        va_start(args, string);
        vsprintf( result, string, args );
        va_end(args);

        if ((okay= strcmp( want, result ) == 0)) {
            printf("\t%s\n", result );
            ++test->index; // advance
        }
        else {
            printf("\tExpected: %s\n\tReceived: %s\n",  want, result );
            test->error= HSM_TRUE;
        }
    }
    return okay;
}

//---------------------------------------------------------------------------
static void Initing( hsm_status status, void * user_data )
{
    if (!(status->hsm->flags & TEST_HSM_NO_LOGGING)) {
        Verify( (test_sequence_t*)user_data, "%s-INIT", status->state->name );
    }        
}

//---------------------------------------------------------------------------
static void Entered( hsm_status status, void * user_data )
{
    if (!(status->hsm->flags & TEST_HSM_NO_LOGGING)) {
        Verify( (test_sequence_t*)user_data, "%s-ENTRY", status->state->name );
    }        
}

//---------------------------------------------------------------------------
static void Exiting( hsm_status status, void * user_data )
{
    if (!(status->hsm->flags & TEST_HSM_NO_LOGGING)) {
        Verify( (test_sequence_t*)user_data, "%s-EXIT", status->state->name );
    }        
}

//---------------------------------------------------------------------------
static void Unhandled( hsm_status status, void * user_data )
{
    if (!(status->hsm->flags & TEST_HSM_NO_LOGGING)) {
        Verify( (test_sequence_t*)user_data, "EVT-%c", ((CharEvent*)status->evt)->ch );
    }        
}

//---------------------------------------------------------------------------
hsm_bool TestEventSequence( hsm_machine hsm, hsm_state first, const char ** string )
{
    hsm_bool test_passed= HSM_FALSE;
    hsm_bool params_okay= hsm && first && string && *string;
    assert( params_okay );
    if (params_okay) {
        hsm_info_t old_callbacks;
        test_sequence_t seq = { string };
        hsm_info_t callbacks= {
            &seq, Initing, Entered, Exiting, Unhandled, 
        };
        HsmSetInfoCallbacks( &callbacks, &old_callbacks );

        printf("  =>" );
        HsmStart( hsm, first );

        while (seq.sequence[ seq.index ] && HsmIsRunning(hsm) && !seq.error) {
            const char * input= seq.sequence[ seq.index++ ];
            CharEvent evt= { *input };
            printf(" %s->\n", input );
            HsmSignalEvent( hsm, &evt );
        }
        test_passed= !seq.error  && (hsm->current != HsmStateError());

        HsmSetInfoCallbacks( &old_callbacks, NULL );
    }        

    return test_passed;
}

