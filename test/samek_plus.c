/**
 * @file samek_plus.c
 *
 * a limitation of samek's original tst is it only uses two leaf states. (s11, and s211)
 * that means there are no transitions where the source of the transition is higher than the lca of current and target.
 * this test adds state s12 and event 'i'

 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include "test.h"
#include <assert.h>
#include <stdio.h>

/*
Samek p.95
  s0:
    - e >> s211.
    - i >> s12.
    s1:
      - a >> s1.                        # self-transition to s1
      - b >> s11.
      - c >> s2.
      - d >> s0.
      - f >> s211.
      s11:
        - g >> s211.
        - h [ foo ] / (foo = 0);
      s12:
    s2:
      - c >> s1.
      - f >> s11.
      s21:
        - b >> s211.
        - h [ !foo ] / (foo = 1); >> s21;
        s211:
        - d >> s21.
        - g >> s0.
*/

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
static const char * Sequence1[] = {
    "s0-ENTRY","s0-INIT","s1-ENTRY","s1-INIT","s11-ENTRY",

    /* s1 handles 'a' in a self-transition, init'ing back down to s11 */
    "a",    "s11-EXIT","s1-EXIT",
            "s1-ENTRY","s1-INIT","s11-ENTRY",

    /* s0 handles 'e' and directs entry down to s211 */
    "e",    "s11-EXIT","s1-EXIT",/*"s0-EXIT", 
                hsm-statechart, doesn't exit the source state, 
                unless the source state target itself.
            "s0-ENTRY",*/"s2-ENTRY","s21-ENTRY","s211-ENTRY",

    /* s0 handles 'e' and directs entry down to s211 again */
    "e",    "s211-EXIT","s21-EXIT","s2-EXIT",/*"s0-EXIT",
            "s0-ENTRY",*/"s2-ENTRY","s21-ENTRY","s211-ENTRY",

    /* unhandled */
    "a",    "EVT-a",        

    /* s21 handles 'h', f==0, guard passes, sets foo, self-transition to s21, inits down to s211 */
    "h",    "s211-EXIT","s21-EXIT",     
            "s21-ENTRY","s21-INIT","s211-ENTRY",

    /* s1 hears 'h', foo==1, guard filters, 'h' is unhandled */
    "h",    "EVT-h",        

    /* s211 handles 'g', directs to 's0', inits down to s11. */
    "g",    "s211-EXIT","s21-EXIT","s2-EXIT",/*"s0-EXIT",
            "s0-ENTRY",*/
            /* a little odd, but since we haven't exited s0, and it's not a leaf state, 
            we get an init without an immediately preceeding enter */
            "s0-INIT","s1-ENTRY","s1-INIT","s11-ENTRY",

    /* s11 handles 'h', clears foo */
    "h",

    /* s0 handles 'i', directs down to 's12' */
    "i",    "s11-EXIT","s1-EXIT",/*"s0-EXIT",
            "s0-ENTRY",*/"s1-ENTRY","s12-ENTRY",

    /* x isn't handled anywhere */
    "x",    "EVT-x",            
};


MAKE_TEST( RunSamekPlus, HSM_TRUE, TestEventSequence, ( hsm, s0(), Sequence1 ) );

int SamekPlusTest()
{
    hsm_context_machine_t machine;
    sp_context_t ctx={0};
    return RunSamekPlus( HsmMachineWithContext( &machine, &ctx.ctx ) );
}

