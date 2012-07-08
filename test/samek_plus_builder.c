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
#include "samek_plus.h"
#include <hsm/builder/hsm_builder.h>

//---------------------------------------------------------------------------
typedef struct sp_context_rec sp_context_t;
struct sp_context_rec {
    hsm_context_t ctx;
    int foo;
};

//---------------------------------------------------------------------------
void SetFoo( hsm_status status, int user_data )    
{
    sp_context_t*sp= (sp_context_t*)status->ctx;
    sp->foo= user_data;
}

hsm_bool TestFoo( hsm_status status, int user_data )    
{
    sp_context_t*sp= (sp_context_t*)status->ctx;
    return sp->foo == user_data;
}

hsm_bool MatchChar( hsm_status status, int user_data )
{
    return status->evt->ch == user_data;
}

#define IfChar( val ) hsmIfUD( (hsm_callback_guard_ud) MatchChar, (void*) val )
#define AndTest( fn, val ) hsmAndUD( (hsm_callback_guard_ud) fn, (void*) val )
#define Run( fn, val ) hsmRunUD( (hsm_callback_guard_ud) fn, (void*) val )

//---------------------------------------------------------------------------
hsm_state buildMachine()
{
    int state=
    hsmBegin( "s0", 0 );
    {
        IfChar( 'e' ); hsmGoto( "s211" );
        IfChar( 'i' ); hsmGoto( "s12" );     
        hsmBegin( "s1", 0 );
        {
            IfChar( 'a' ); hsmGoto( "s1" );
            IfChar( 'b' ); hsmGoto( "s11" );
            IfChar( 'c' ); hsmGoto( "s2" );
            IfChar( 'd' ); hsmGoto( "s0" );
            IfChar( 'f' ); hsmGoto( "s211" );
            hsmBegin( "s11", 0 );
            {
                IfChar( 'g' ); 
                    hsmGoto( "s211" );

                IfChar( 'h' ); 
                AndTest( TestFoo, 1 ); 
                    Run( SetFoo, 0 );
            }
            hsmEnd();

            hsmBegin( "s12", 0 );
            hsmEnd();
        }
        hsmEnd();
        hsmBegin( "s2", 0 );
        {
            IfChar( 'c' ); hsmGoto( "s1" );
            IfChar( 'f' ); hsmGoto( "s11" );
            hsmBegin( "s21", 0 );
            {
                IfChar( 'b' ); 
                    hsmGoto( "s211" );

                IfChar( 'h' ); 
                AndTest(  TestFoo, 0 );
                    Run( SetFoo, 1 );
                    hsmGoto( "s21" );

                hsmBegin( "s211", 0 );
                {
                    IfChar( 'd' ); hsmGoto( "s21" );
                    IfChar( 'g' ); hsmGoto( "s0" );
                }
                hsmEnd();
            }
            hsmEnd();
        }
        hsmEnd();
    }
    hsmEnd();
    return hsmResolveId( state );
}

//---------------------------------------------------------------------------
hsm_bool SamekPlusBuilderTest()
{
    hsm_bool res;
    hsm_context_machine_t machine;
    sp_context_t ctx={0};

    hsmStartup();
    res= TestEventSequence( 
            HsmMachineWithContext( &machine, &ctx.ctx ), 
            buildMachine(), 
            SamekPlusSequence() );

    hsmShutdown();
    return res;
}
