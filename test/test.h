/**
 * @file hsm_forwards.h
 *
 * helps unwind dependencies b/t hsm header definitions in a friendly way.
 *
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * All code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#pragma once
#ifndef __TEST_H__
#define __TEST_H__

#include "hsm_machine.h"

//---------------------------------------------------------------------------
typedef struct hsm_event_rec CharEvent;
struct hsm_event_rec {
    char ch;
};

//---------------------------------------------------------------------------
/**
 * TestEventSequence
 *
 * @param hsm machine to test
 * @param seq array of strings
 *
 * seq is a null-terminated string in the format: "expected output", "input", "expected output", ... NULL
 */
hsm_bool TestEventSequence( hsm_machine hsm, hsm_state first, const char ** seq );

/**
 * MAKE_TEST( name, want, fn, (parms) )
 *
 * @param name Name of function, used a string for console output.
 * @param want Boolean value desired.
 * @param fn   Test function to call.
 * @param params Parameters for the passed fn.
 *
 * @return a function which takes an hsm_machine, and which will return a 0 on success, -1 on error
 */
#define MAKE_TEST( name, want, fn, parms ) \
    int name(hsm_machine hsm) {            \
        hsm_bool res;                      \
        printf( #name "\n" );              \
        res= fn parms == want;                    \
        printf( #name " %s\n", res ? "passes." : "FAILS." ); \
        return res ? 0 : -1; \
    }

// a state with nothing to do
#define EMPTY_STATE( s,p,i ) \
    HSM_STATE( s, p, i ); \
    hsm_state s##Event( hsm_machine hsm, hsm_context ctx, hsm_event evt ) {return NULL; }


#endif // #ifndef __TEST_H__
