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
#include "hsm_builder.h"

//---------------------------------------------------------------------------
typedef struct sp_context_rec sp_context_t;
struct sp_context_rec {
    hsm_context_t ctx;
    int foo;
};

//---------------------------------------------------------------------------
void ToggleFoo( hsm_status status, int  user_data )    
{
    sp_context_t*sp= (sp_context_t*)status->ctx;
    sp->foo= !sp->foo;
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

//---------------------------------------------------------------------------
hsm_state buildMachine()
{
    int state=
    hsmBegin( "s0" );
    {
        hsmOn( MatchChar, 'e' ); hsmGoto( "s211" );
        hsmOn( MatchChar, 'i' ); hsmGoto( "s12" );     
        hsmBegin( "s1" );
        {
            hsmOn( MatchChar, 'a' ); hsmGoto( "s1" );
            hsmOn( MatchChar, 'b' ); hsmGoto( "s11" );
            hsmOn( MatchChar, 'c' ); hsmGoto( "s2" );
            hsmOn( MatchChar, 'd' ); hsmGoto( "s0" );
            hsmOn( MatchChar, 'f' ); hsmGoto( "s211" );
            hsmBegin( "s11" );
            {
                hsmOn( MatchChar, 'g' ); 
                    hsmGoto( "s211" );

                hsmOn( MatchChar, 'h' ); 
                    hsmIf( TestFoo, 1 ); 
                    hsmRun( ToggleFoo, 0 );
            }
            hsmEnd();

            hsmBegin( "s12" );
            hsmEnd();
        }
        hsmEnd();
        hsmBegin( "s2" );
        {
            hsmOn( MatchChar, 'c' ); hsmGoto( "s1" );
            hsmOn( MatchChar, 'f' ); hsmGoto( "s11" );
            hsmBegin( "s21" );
            {
                hsmOn( MatchChar, 'b' ); 
                    hsmGoto( "s211" );

                hsmOn( MatchChar, 'h' ); 
                    hsmIf(  TestFoo, 0 );
                    hsmRun( ToggleFoo, 0 );
                    hsmGoto( "s21" );

                hsmBegin( "s211" );
                {
                    hsmOn( MatchChar, 'd' ); hsmGoto( "s21" );
                    hsmOn( MatchChar, 'g' ); hsmGoto( "s0" );
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

