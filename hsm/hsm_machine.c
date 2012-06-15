/**
 * @file hsm_machine.c
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
#include "hsm_state.h"
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
    static hsm_state _##Pseudo( struct hsm_machine* hsm, struct hsm_context*ctx, struct hsm_event*evt ) { \
        return Pseudo(); \
    } \
    hsm_state Pseudo() { \
        static struct hsm_state_rec info= { #Pseudo, _##Pseudo };\
        return &info;   \
    }

PSEUDO_STATE( HsmStateTerminated );
PSEUDO_STATE( HsmStateError );
PSEUDO_STATE( HsmStateHandled );

static hsm_state HsmDoNothing( struct hsm_machine* hsm, struct hsm_context*ctx, struct hsm_event*evt ) { 
    return NULL; 
}

hsm_state HsmTopState() { 
    static struct hsm_state_rec top_state= { "HsmTopState", HsmDoNothing };
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
static hsm_bool HsmTransition( hsm_machine_t*, hsm_state, hsm_state, struct hsm_event* );

/**
 * internal function:
 * Used to start the statemachine going
 * walks up the tree to the top, then 'enter's from top down-to (and including) 'state'
 
 * @param machine Machine that is transitioning
 * @param state Desired first state of the state chart
 */
static void HsmRecursiveEnter( hsm_machine_t*, hsm_state, struct hsm_event* );

/**
 * internal function: 
 * Enter the passed state 
 * @param machine Machine that is transitioning
 * @param state State transitioing to
 * @param evt Event which caused the transition
 */
static void HsmEnter( hsm_machine_t*, hsm_state, struct hsm_event* );

/* 
 * internal function:
 * Transitions to the current states tree of initial states
 * @return HSM_FALSE on error.
 * @note in statecharts, init happens after enter, and it can move ( often does move ) the current state to a new state
 */
static void HsmInit( hsm_machine_t*, struct hsm_event*);

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
hsm_bool HsmIsInState( hsm_machine_t * hsm, hsm_state state )
{
    hsm_bool res=HSM_FALSE;
    if (hsm && state) {
        hsm_state test;
        for (test= hsm->current;  test; test=test->parent) {
            if (res=(test==state)) {
                break;
            }
        }
    }        
    return res;
}

//---------------------------------------------------------------------------
hsm_bool HsmStart( hsm_machine_t* hsm, hsm_context_t* ctx, hsm_state first_state  )
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
        HsmRecursiveEnter( hsm, first_state, NULL  );

        // statecharts run enter *then* init
        // ( note: init can move us into a new state )
        HsmInit( hsm, NULL );
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
        hsm_state handler= hsm->current;
        hsm_state next_state;
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
                 HsmInit( hsm, evt );
             }
        }
    }    
    return okay;
}

//---------------------------------------------------------------------------
static void HsmInit( hsm_machine_t*hsm, struct hsm_event * cause )
{
    //FIXME-stravis: handle invalid states NULL process pointer in someway?
    hsm_state_fn want_state;
    while ( want_state= hsm->current->initial ) 
    {
        const hsm_state initial_state= hsm->current->initial();
        hsm_bool init_moves_to_child= initial_state->parent == hsm->current;
        assert( init_moves_to_child && "malformed statechart: init doesnt move to child state" );
        if (!init_moves_to_child) {
            hsm->current= HsmStateError();
            break;
        }
        else {
            // continue the enter=>init pattern ( the spec says enter runs before init ( which sounds insane ) but works out well. )
            // ( each new state gets the context of its parent and generates a new context in turn )
            HsmEnter( hsm, initial_state, cause );
        }
    }
}

//---------------------------------------------------------------------------
static void HsmEnter( hsm_machine_t* hsm, hsm_state state, struct hsm_event* cause )
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
    hsm_state state= hsm->current;
    hsm_context_t* ctx= hsm->stack ? hsm->stack->context :NULL;
    if (state->exit) {
        state->exit( hsm, ctx, cause );
    }
    HsmContextPop( hsm->stack );
    hsm->current= state->parent;
}

//---------------------------------------------------------------------------
static void HsmRecursiveEnter( hsm_machine_t*hsm, hsm_state state, struct hsm_event *cause )
{
    if (state) {
        HsmRecursiveEnter( hsm, state->parent, cause);
        HsmEnter( hsm, state, cause );
    }
}

//---------------------------------------------------------------------------
/**
 According to the http://www.w3.org/TR/scxml/#SelectingTransitions:
 I. """ The behavior of a transition with 'type' of "external" (the default) is defined in terms of the transition's source state 
   (which is the state that contains the transition), the transition's target state(or states), 
    and the Least Common Compound Ancestor (LCCA) of the source and target states 
    When a transition is taken, the state machine will exit all active states that are proper descendants of the LCCA, 
    starting with the innermost one(s) and working up to the immediate descendant(s) of the LCCA. 

    Then the state machine enters the target state(s), plus any states that are between it and the LCCA, 
    starting with the outermost one (i.e., the immediate descendant of the LCCA) and working down to the target state(s). 
     
    As states are exited, their <onexit> handlers are executed. Then the executable content in the transition is executed, 
    followed by the <onentry> handlers of the states that are entered. If the target state(s) of the transition is not atomic, 
    the state machine will enter their default initial states recursively until it reaches an atomic state(s).

    Note that the LCCA is neither entered nor exited."""
    
 II. When target is a descendant of source....
     "an internal transition will not exit and re-enter its source state, while an external one will."

 III. """ [For] a <transition> whose target is its source state.... the state is exited and reentered, 
     triggering execution of its <onentry> and <onexit> executable content."""
 */
#define ERROR_IF_NULL( x, msg ) while(!x) { assert(msg); return HSM_FALSE; }

static hsm_bool HsmTransition( hsm_machine_t* hsm, hsm_state source, hsm_state target, struct hsm_event* cause )
{
    // above ( I. ) specifically designates the lca of 'source' and 'target'.
    // which can only, then, be the same node as 'source' or, more likely, a node less deep than 'source'.
    // since 'source' is the node that handled the event, and since events propogate from 'current' up to the top of the tree 
    // that also means 'source' is either the same node as, or higher than, 'current'.
    // ie. lca->depth <= source->depth <= current->depth
    // therefore: as a first step, bring 'current' to 'source', and work towards lca from there.
    while( hsm->current != source ) {
        HsmExit( hsm, cause );
        ERROR_IF_NULL( hsm->current, "jumped past top" );
    }                

    // quick check for self transition: the source targeted itself ( III. above )
    if ( hsm->current == target ) {
        HsmExit( hsm, cause );
        // ( <--- standard transitions would take place here )
        HsmEnter( hsm, target, cause );
    }
    else {
        // the easiest entry path record is an object on the stack... 
        // doesnt worry too much about stack overflow, 
        // instead it lets the user control it via HSM_MAX_DEPTH...
        int pt=0;
        hsm_state track= target;
        hsm_state* targets=(hsm_state*) alloca( target->depth * sizeof(hsm_state) );
        hsm_bool external_transition= 0;
        ERROR_IF_NULL( targets, "out of space" );
 
        // source deep than target?
        if (hsm->current->depth > track->depth) {
            // *exit* up to the same level
            while( hsm->current->depth > track->depth) {
                HsmExit( hsm, cause ); // we exit, then move, b/c, if we don't leave the node we don't want an exit 
                ERROR_IF_NULL( hsm->current, "jumped past top" );  
            }
        }
        // target deeper than source?
        else {
            // *record* its path up to the same level
            while (track->depth > hsm->current->depth ) {
                targets[pt++]= track;
                track= track->parent;
                ERROR_IF_NULL( track, "jumped past top" );
            }

            // if target has risen now to the level of source, 
            // and they re the same node, then source was an ancestor of target.
            if (hsm->current == track) {
                // II. trigger an external transitions like this:
                // HsmExit( hsm, cause ); 
                // track= hsm->current;
            }
        }
            
        // keep going up together till current and track have found each other
        // ( keep exiting 'current' as it goes up, and keep recording 'track' as *it* goes up )
        while (hsm->current!= track) {
            HsmExit( hsm, cause ); 
            targets[pt++]= track;
            track= track->parent;
            ERROR_IF_NULL( hsm->current&&track, "jumped past top" );
        }            

        // ( <--- standard transitions would take place here )

        // now have current use path the we just recorded:
        // it's turtles all the way down.
        while (pt>0) {
            track= targets[--pt];
            HsmEnter( hsm, track, cause );
        }
    }
    // note: on error we will have already returned HSM_FALSE
    return HSM_TRUE;
}
