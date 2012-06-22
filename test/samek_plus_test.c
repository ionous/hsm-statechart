/**
 * @file samek_plus.c
 *
 * Use HSM_STATE macros to define an implementation of samek's test.
 *
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include "test.h"
#include <stdio.h>
#include "samek_plus.h"

//---------------------------------------------------------------------------
typedef struct sp_context_rec sp_context_t;
struct sp_context_rec {
    hsm_context_t ctx;
    int foo;
};

//---------------------------------------------------------------------------
// samek plus test
HSM_STATE( s0, HsmTopState, s1 );

    HSM_STATE( s1, s0, s11  );
        HSM_STATE( s11, s1, 0 );
        HSM_STATE( s12, s1, 0 );

    HSM_STATE( s2, s0, s21  );
        HSM_STATE( s21, s2, s211  );
            HSM_STATE( s211, s21, 0 );

//---------------------------------------------------------------------------
hsm_state s0Event( hsm_machine hsm, hsm_context ctx, const CharEvent*evt ) 
{
    switch (evt->ch) {
        case 'e': return s211();
        case 'i': return s12();
    }
    return 0;
}

hsm_state s1Event( hsm_machine hsm, hsm_context ctx, const CharEvent*evt ) 
{
    switch (evt->ch) {
        case 'a': return s1();
        case 'b': return s11();
        case 'c': return s2();
        case 'd': return s0();
        case 'f': return s211();
     }
     return 0;
}

hsm_state s11Event( hsm_machine hsm, hsm_context ctx, const CharEvent*evt ) 
{
    switch (evt->ch) {
        case 'g': return s211();
        case 'h': {
            if ((( sp_context_t*)ctx)->foo) {
                (( sp_context_t*)ctx)->foo =0;
                return HsmStateHandled();            
            }
            break;
        }
    }
    return 0;
}
hsm_state s12Event( hsm_machine hsm, hsm_context ctx, const CharEvent*evt ) {
    switch (evt->ch) {
        case 'e': return s211();
        case 'i': return s12();
    }
    return 0;
}

hsm_state s2Event( hsm_machine hsm, hsm_context ctx, const CharEvent*evt ) {
    switch (evt->ch) {
        case 'c': return s1();
        case 'f': return s11();
    }
    return 0;
}
hsm_state s21Event( hsm_machine hsm, hsm_context ctx, const CharEvent*evt ) {
    switch (evt->ch) {
        case 'b': return s211();
        case 'h': {
            if (!(( sp_context_t*)ctx)->foo) {
                (( sp_context_t*)ctx)->foo=1;
                return s21();
            }
            break;
        }            
    }
    return 0;
}

hsm_state s211Event( hsm_machine hsm, hsm_context ctx, const CharEvent*evt ) {
    switch (evt->ch) {
        case 'd': { 
            return s21();
        }
        case 'g': {
            return s0();
        }            
    }
    return 0;
}

//---------------------------------------------------------------------------
/**
 * 
 */
int SamekPlusTest()
{
    hsm_context_machine_t machine;
    sp_context_t ctx={0};
    return TestEventSequence( HsmMachineWithContext( &machine, &ctx.ctx ), s0(), SamekPlusSequence() );
}
