/**
 * hsm_machine.c
 *
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * All code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include "hsm_machine.h"

#include <assert.h>
#include <stdlib.h>
#include <memory.h>

#include "hsm_context.h"
#include "hsm_info.h"
#include "hsm_stack.h"

// alloca is technically not an ANSI-C function, though it exists on most platforms
#ifdef WIN32
#ifndef alloca
#define alloca _alloca 
#endif
#endif

/**
 * macro for context stack refuseniks
 */
#define HSM_CONTEXT( hsm ) ((hsm)->stack? (hsm)->stack->context: NULL)

/**
 * macro for making a pseudostate:
 * 1. a function that does nothing
 * 2. a state info function to describe that state
 */
#define PSEUDO_STATE( Pseudo ) \
    static hsm_info_t* _##Pseudo(struct hsm_machine *machine, struct hsm_context*ctx, struct hsm_event*evt ) { \
        return Pseudo(); \
    } \
    hsm_info_t * Pseudo() { \
        static hsm_info_t info= { #Pseudo, _##Pseudo };\
        return &info;   \
    }

PSEUDO_STATE( HsmStateTerminated );
PSEUDO_STATE( HsmStateError );
PSEUDO_STATE( HsmStateHandled );

static hsm_info_t* HsmDoNothing(struct hsm_machine *machine, struct hsm_context*ctx, struct hsm_event*evt ) { 
    return NULL; 
}

hsm_info_t * HsmTopState() { 
    static hsm_info_t top_state= { "HsmTopState", HsmDoNothing };
    return &top_state;
}


/**
 * internal function:
 * @param hsm Machine transitioning
 * @param source State causing the transition
 * @param target State the machine is moving into
 * @param evt Event which caused the transition
 * @warning This modifies the stack and the machine as it processes
 */
static hsm_bool HsmTransition( hsm_machine_t*, hsm_info_t* , hsm_info_t* , struct hsm_event* );

/**
 * internal function:
 * Used to start the statemachine going
 * walks up the tree to the top, then 'enter's from top down-to (and including) 'state'
 
 * @param machine Machine that is transitioning
 * @param state Desired first state of the state chart
 */
static void HsmRecursiveEnter( hsm_machine_t*, hsm_info_t *);

/**
 * internal function: 
 * Enter the passed state 
 * @param machine Machine that is transitioning
 * @param state State transitioing to
 * @param evt Event which caused the transition
 */
static void HsmEnter( hsm_machine_t*, hsm_info_t*, struct hsm_event* );

/* 
 * internal function:
 * Sends 'init' to the current state
 * @return HSM_FALSE on error.
 * @note in statecharts, init happens after enter, and it can move ( often does move ) the current state to a new state
 */
static void HsmInit( hsm_machine_t*hsm );

/**
 * internal function:
 * Exit the current state
 * @param machine Machine that is transitioning
 * @param evt Event which caused the transition
 */
static void HsmExit( hsm_machine_t*, struct hsm_event* );

//---------------------------------------------------------------------------
hsm_machine_t* HsmMachine( hsm_machine_t * hsm, hsm_context_stack_t* stack, hsm_callback_context_popped on_popped )
{
    assert (hsm);
    if (hsm) {
        hsm->current= NULL;
        hsm->init= NULL;
        hsm->on_unhandled_event= NULL;
        hsm->stack= HsmContextStack( stack, on_popped );
    }
    return hsm;
}

//---------------------------------------------------------------------------
hsm_bool HsmIsRunning( hsm_machine_t * hsm )
{
    return hsm && ((hsm->current != HsmStateTerminated()) &&
                   (hsm->current != HsmStateError()));
}

//---------------------------------------------------------------------------
hsm_bool HsmIsInState( hsm_machine_t * hsm, hsm_info_t* state )
{
    hsm_bool res=HSM_FALSE;
    if (hsm && state) {
        hsm_info_t * test;
        for (test= hsm->current;  test; test=test->parent) {
            if (res=(test==state)) {
                break;
            }
        }
    }        
    return res;
}

//---------------------------------------------------------------------------
hsm_bool HsmStart( hsm_machine_t* hsm, hsm_context_t* ctx, hsm_info_t *first_state  )
{
    assert( hsm );
    assert( first_state && "expected valid first state for init" );
    assert( (!hsm || !hsm->current) && "already ran init" );
    
    if (hsm && !hsm->current && first_state ) 
    {
        // add the outermost context
        HsmContextPush( hsm->stack, ctx );
        
        // the specified starting state isn't necessarily the *top* state:
        // we need to walk up to the top state, then walk down to and including our first state
        HsmRecursiveEnter( hsm, first_state  );

        // statecharts run enter *then* init
        // ( note: init can move us into a new state )
        HsmInit( hsm );
    }

    return HsmIsRunning( hsm );
}

//---------------------------------------------------------------------------
hsm_bool HsmProcessEvent( hsm_machine_t* hsm, struct hsm_event* evt )
{
    hsm_bool okay= HSM_FALSE;
    if (hsm && hsm->current) 
    {
        // bubble the event up the hierarchy until we get a valid respose 
        // ( or until we run off the top of the tree. )
        hsm_info_t * handler= hsm->current;
        hsm_info_t * next_state;
        hsm_context_iterator_t it;
        HsmContextIterator( hsm->stack, &it );

        // note: in theory, to have gotten this far with the current handler, 'process' must have existed.
        next_state= handler->process( hsm, it.context, evt );
        while (!next_state && ((handler= handler->parent) != NULL)) 
        {
            if (!handler->process) {
                next_state= HsmStateError();
            }
            else {
                next_state= handler->process( hsm, HsmParentContext( &it ), evt );
            }
        }            

        // handlers are supposed to return HsmStateHandled
        if (!next_state) {
            if (hsm->on_unhandled_event) {
                hsm->on_unhandled_event( hsm, evt );
            }
        }
        else 
        if (next_state== HsmStateHandled()) {
            okay= HSM_TRUE;
        }
        else 
        if (next_state== HsmStateTerminated() || next_state==HsmStateError()) {
            // we say this is okay, in the sense that something happened.
            // trigger a callback to inform the user?
            if (hsm->current != next_state) {
                hsm->current = next_state;
                okay= HSM_TRUE;
            }
        }
        else {
            // transition, and if all is well, init
             if (!HsmTransition( hsm, handler, next_state, evt )) {
                hsm->current= HsmStateError();
             }
             else {
                 HsmInit(hsm);
             }
        }
    }    
    return okay;
}

//---------------------------------------------------------------------------
static void HsmInit( hsm_machine_t*hsm )
{
    //FIXME-stravis: handle invalid states NULL process pointer in someway?
    hsm_info_t* want_state;
    while ( want_state= hsm->current->process( hsm, HSM_CONTEXT(hsm), hsm->init ) ) 
    {
        hsm_bool init_moves_to_child= want_state->parent == hsm->current;
        assert( init_moves_to_child && "malformed statechart: init doesnt move to child state" );
        if (!init_moves_to_child) {
            hsm->current= HsmStateError();
            break;
        }
        else {
            // continue the enter=>init pattern ( the spec says enter runs before init ( which sounds insane ) but works out well. )
            // ( each new state gets the context of its parent and generates a new context in turn )
            HsmEnter( hsm, want_state, hsm->init );
        }
    }
}

//---------------------------------------------------------------------------
static void HsmEnter( hsm_machine_t* hsm, hsm_info_t* state, struct hsm_event* cause )
{
    //FIXME-stravis: handle invalid states better?
    const hsm_bool valid_state= (state && state->depth >=0 && state->process);
    assert( valid_state );
    if (valid_state) {
        // i dont like that this directly alters current, but then the push *does* directly modify the stack, so....
        // more importantly: since we are passing hsm into 'enter' i can imagine people might want to access their own state's info pointer
        // 
        // note: i've now adapted other functions ( inner, exit, transition ) to this same "alter current" pattern 
        // ( and it does make things cleaner )
        hsm_context_t* ctx= HSM_CONTEXT( hsm );
        hsm->current= state;
    
        if (state->enter) {
            ctx= state->enter( hsm, ctx, cause );
        }
        HsmContextPush( hsm->stack, ctx );    
    }
}

//---------------------------------------------------------------------------
static void HsmExit( hsm_machine_t*hsm, struct hsm_event* cause )
{
    hsm_info_t* state= hsm->current;
    hsm_context_t* ctx= hsm->stack ? hsm->stack->context :NULL;
    if (state->exit) {
        state->exit( hsm, ctx, cause );
    }
    HsmContextPop( hsm->stack );
    hsm->current= state->parent;
}

//---------------------------------------------------------------------------
static void HsmRecursiveEnter( hsm_machine_t*hsm, hsm_info_t * state )
{
    if (state) {
        HsmRecursiveEnter( hsm, state->parent );
        HsmEnter( hsm, state, hsm->init );
    }
}

//---------------------------------------------------------------------------
// 
// it's not purely that we need to find an lca
// what we really to do is:
//  1. exit up to, but not including, the source of the transition
//  2. exit up to, but not including, the least common ancestor 
//  3. enter down to the target
//
// note: the "not including" is because we treat all transitions as internal
// 
#define ERROR_IF_NULL( x, msg ) while(!x) { assert(msg); return HSM_FALSE; }

static hsm_bool HsmTransition( hsm_machine_t* hsm, hsm_info_t* source, hsm_info_t* next, struct hsm_event* cause )
{
    // exit up to, but not including, the node that asked for the transition
    // FIXME-stravis: is this actually right? i need to look at the documenation....
    // shouldn't  the root node be able to enact transitions down below without disrupting the machine?
    while( hsm->current != source ) {
        HsmExit( hsm, cause );
        ERROR_IF_NULL( hsm->current, "jumped past top" );
     }                
 
    // quick check for self transition
    if ( hsm->current == next ) {
        HsmExit( hsm, cause );
        HsmEnter( hsm, next, cause );
    }
    else {
        // the easiest entry path record is an object on the stack... 
        // doesnt worry too much about stack overflow, 
        // instead it lets the user control it via HSM_MAX_DEPTH...
        int pt=0;
        hsm_info_t** targets=(hsm_info_t**) alloca( next->depth * sizeof(hsm_info_t*) );
        ERROR_IF_NULL( targets, "out of space" );

        // in case current started deeper than next, *exit* up to the same level
        while( hsm->current->depth > next->depth) {
            HsmExit( hsm, cause ); // we exit, then move, b/c, if we don't leave the node we don't want an exit 
            ERROR_IF_NULL( hsm->current, "jumped past top" );     // and, while our only shared ancestor might be "top", we dont want to exit top.
        }

        // otherwise: while next is deeper than current, bring it up and *record* its path.
        while( next->depth > hsm->current->depth ) {
            targets[pt++]= next;
            next= next->parent;
            ERROR_IF_NULL( next, "jumped past top" );
        }

        // now, we're at the same depth, but we we might be in different branches of the tree
        // keep going up together till current and next have found each other
        // ( exit 'current' as it goes up, and record 'next' as *it* goes up )
        while (hsm->current!= next) {
            HsmExit( hsm, cause ); 
            targets[pt++]= next;
            next= next->parent;
            ERROR_IF_NULL( hsm->current&&next, "jumped past top" );
        }

        // now have current use path the we just recorded:
        // it's turtles all the way down.
        while (pt>0) {
            next= targets[--pt];
            HsmEnter( hsm, next, cause );
        }
    }

    // note: on error we will have already returned HSM_FALSE
    return HSM_TRUE;
}
