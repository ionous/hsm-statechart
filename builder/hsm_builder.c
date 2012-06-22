/**
 * @file hsm_builder.c
 *
 * \internal
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include <hsm/hsm_machine.h>
#include "hash.h"
#include "hsm_builder.h"
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>

extern const hsm_uint32 HsmLowerTable[];

//---------------------------------------------------------------------------
// FNVa is in the public domain, http://isthe.com/chongo/tech/comp/fnv/
// http://dev.ionous.net/2007/03/string-hashes.html for more about string hashing
#define CRC32(x) ComputeFNVa32( x, 0x811c9dc5 )
#define CRC32_CAT(x,v) ComputeFNVa32( x, v )

hsm_uint32 ComputeFNVa32(const char *string, hsm_uint32 hval) 
{
    const hsm_uint32 prime32= ((hsm_uint32)0x01000193);
    const char *p=string;
    if (p) {
        hsm_uint32 ch;
        for (ch=*p; ch; ch=*(++p)) {
            hval= (hval ^ HsmLowerTable[ch]) * prime32;
        }
    }        
    return hval;
}

// the things we build:
typedef struct state_rec    state_t;
typedef struct guard_rec    guard_t;
typedef struct action_rec   action_t;
typedef struct handler_rec  handler_t;

//---------------------------------------------------------------------------
/**
 * one each state defined via the hsmBuilder interface.
 * it augments the 'common' state descriptor.
 * with data to generically handle events, guards, actions, and transitions.
 */
struct state_rec
{
    hsm_context_t ctx;          // we push them on hsmBegin, pop them on hsmEnd
                                // it's reused to indicate build completion status
                                // null => hasnt been the subject of an hsmBegin
                                // self => finished hsmEnd
                                // anything else => in progress
    hsm_state_t desc;           // the common statedescriptor
    hsm_uint32  id;             // unique id for searching for this state
    handler_t * handlers;   // list of events to handle
};

// querries for build status
#define State_ReadyToBuild( state )     ((state) && !(state)->ctx.parent)
#define State_FinishedBuilding( state ) ((state) && ((state)->ctx.parent == &((state)->ctx)))
#define State_BuildInProgress( state )  ((state) &&  (state)->ctx.parent && ((state)->ctx.parent != &((state)->ctx)))

//---------------------------------------------------------------------------
/**
 * 
 */
struct action_rec
{
    hsm_callback_action run; // the action is the same as 'handler', the caller can translate from there.
    int action_data;         // passed to run
    action_t* next;           // we are a linked list, b/c there can be more than one action.
};


//---------------------------------------------------------------------------
/**
 * eventually want a full blown decision tree to allow ands and ors
 * right now, it's all ands
 */
struct guard_rec
{
    hsm_callback_guard match;    // function to determine if this rec handles some any particular event
    int guard_data;           // passed to match    
    guard_t * next;
};

//---------------------------------------------------------------------------
/**
 * event handleror data.
 * one each event defined via the hsmBuilder interface.
 * may well be used for guards and events.
 */
struct handler_rec
{
    guard_t guard;
    action_t *actions;      // if we do match: we may have things to do
    hsm_state target;       //             not to mention, places to go.
    handler_t  *next;         // if not us, who? if not now, next?
};


//---------------------------------------------------------------------------
/**
 * run time helper used via state_t
 */
static hsm_state GenericEvent( hsm_status status )
{
    // ways of getting the state_rec
    // 1. override enter and use context [ would limit the depth of context stack; would limit mixing of user code ]
    // 2. override enter and use a separate linked list [ would limit mixing ]
    // 3. pointer math on current state.
    hsm_state ret=NULL;
    state_t* state= (state_t*) ( ((size_t)status->state) -  offsetof( state_t, desc ) );
    handler_t* et;
    // look through the built event handlers
    for (et= state->handlers; et; et=et->next) { 
        // determine if some guard blocks the event from running
        guard_t* guard;
        for (guard= &(et->guard); guard; guard=guard->next) {
            if (!guard->match( status, guard->guard_data )) {
                break;
            }
        }
        // nope: no guard blocks this event from running
        if (!guard) {
            // run action(s)
            action_t* at;
            for (at= et->actions; at; at=at->next) {
                at->run( status, at->action_data );
            }
            // transition to target, or flag as handled.
            if (et->target) {
                ret=et->target;
            }
            else {
                ret= HsmStateHandled();
            }
            break;
        }
    }
    return ret;
}

//---------------------------------------------------------------------------
/**
 * @internal construct a new state object
 */
static state_t* NewState( const char * name, hsm_uint32 id )
{   
    state_t* state=(state_t*) calloc( 1, sizeof( state_t ) );
    if (state) {
        state->id= id;
        state->desc.name= name;
    }
    return state;
}

//---------------------------------------------------------------------------
/**
 * @internal construct a new event object
 */
static handler_t* NewHandler( state_t* state, hsm_callback_guard match, int guard_data )
{   
    handler_t* handler= NULL;
    if (state && match) {
        handler= (handler_t*) calloc( 1, sizeof( handler_t ) );
        if (handler) {
            // setup the first guard:
            handler->guard.match= match;
            handler->guard.guard_data= guard_data;
            // link to the list of handlers for this state:
            handler->next= state->handlers;
            state->handlers= handler;
        }        
    }        
    return handler;
}

//---------------------------------------------------------------------------
/**
 * @internal construct a new action object
 */
static action_t* NewAction( handler_t* handler, hsm_callback_action run, int action_data )
{
    action_t* action= NULL;
    if (handler && run) {
        action= (action_t*) calloc( 1, sizeof( action_t ) );
        if (action) {
            // set:
            action->run= run;
            action->action_data= action_data;
            //link:
            action->next= handler->actions;
            handler->actions= action;
        }
    }        
    return action;
}

//---------------------------------------------------------------------------
/**
 * @internal construct a new guard
 */
static guard_t* NewGuard( handler_t* handler, hsm_callback_guard match, int guard_data )
{   
    guard_t* guard= NULL;
    if (handler && match) {
        guard= (guard_t*) calloc( 1, sizeof( guard_t ) );
        if (guard) {
            // set the default matching function
            guard->match= match;
            guard->guard_data= guard_data;
            // WARNING: link order doesnt matter right now because they are all "and" but it will for "or" and "and"
            guard->next= handler->guard.next;
            handler->guard.next= guard;
        }            
    }        
    return guard;
}

//---------------------------------------------------------------------------
// Builder Machine
//---------------------------------------------------------------------------

typedef struct builder_rec builder_t;
typedef enum builder_events builder_events_t;
typedef struct hsm_event_rec BuilderEvent;

/**
 * the handlers that the builder interface uses to build hsms.
 * ~= one per interface function.
 * why *shouldn't* a statemachine build a statemachine after all?
 */
enum builder_events {
    _hsm_begin,
    _hsm_end,
    _hsm_guard,
    _hsm_enter,
    _hsm_goto,
    _hsm_action,
    // _hsm_exit,
};

/**
 * the builder interface is secretly backed by one of these.
 */
struct builder_rec
{
    hsm_context_t ctx;
    const char *error;
    hsm_context_stack_t stack;  // a stack of handlering states.
    hash_table_t hash;          // a hash of states
};


/**
 * builder_rec helper macro to return the state that's currently getting built
 */
#define Builder_CurrentState( b ) (state_t*)((b)->stack.context)
#define Builder_FindState( b, id ) (state_t*)Hash_FindData( &((b)->hash), id ) 

/**
 * builder_rec helper macro to record an error string
 */
#define Builder_Error( b, e ) (b)->error=e

//---------------------------------------------------------------------------
/**
 * data associated with the builder_events_t
 * essentially: the interface functions repacked a function object.
 */
struct hsm_event_rec
{
    builder_events_t type;
    union _data {
        state_t * state;
        hsm_callback_enter enter;
        //hsm_callback_exit exit;
        hsm_callback_action run;
        hsm_callback_guard guard;
        hsm_state go;
    }
    data;
};

typedef struct hsm_guard_rec GuardEvent;
struct hsm_guard_rec
{
    BuilderEvent core;
    int guard_data;   
    hsm_bool append;
};


typedef struct hsm_action_rec ActionEvent;
struct hsm_action_rec
{
    BuilderEvent core;
    int action_data;
};


HSM_STATE( Building, HsmTopState, BuildingIdle );
    HSM_STATE( BuildingIdle, Building, 0 );
    HSM_STATE_ENTER( BuildingState, Building, BuildingBody );
        HSM_STATE( BuildingBody, BuildingState, 0 );
        HSM_STATE_ENTER( BuildingGuard, BuildingState, 0 );


//---------------------------------------------------------------------------
// Build:
//---------------------------------------------------------------------------
static hsm_state BuildingEvent( hsm_status status )
{
    hsm_state ret= HsmStateError(); // at the top level, by default, everythings an error.
    builder_t * builder= ((builder_t*)status->ctx);
    switch (status->evt->type) {
        case _hsm_begin: {
            state_t * state= status->evt->data.state;
            if (State_ReadyToBuild( state )) {
                ret= BuildingState();
            }
            else {
                if (!state) {
                    Builder_Error( builder, "hsmBegin for unknown state." );
                }
                else 
                if (State_FinishedBuilding( state )) {
                    Builder_Error( builder, "state already finished building via hsmEnd.");    
                }
                else {
                    Builder_Error( builder, "state already being built via hsmBegin.");
                }
            }
        }            
        break;    
        default:
            Builder_Error( builder, "invalid function for state" );
        break;
    }
    return ret;
}

//---------------------------------------------------------------------------
// Idle:
//---------------------------------------------------------------------------
static hsm_state BuildingIdleEvent( hsm_status status )
{
    return NULL;  
}

//---------------------------------------------------------------------------
// State:
//---------------------------------------------------------------------------
static hsm_state BuildingBodyEvent(hsm_status status )
{   
    return NULL;
}

//---------------------------------------------------------------------------
static hsm_context BuildingStateEnter( hsm_status status )
{
    builder_t * builder= ((builder_t*)status->ctx);
    state_t* current= Builder_CurrentState( builder );
    state_t* new_state= status->evt->data.state;
    assert( status->evt->type == _hsm_begin );
    assert( !new_state->desc.process );

    // this is pretty poor verification, b/c we cant use non-builder states
    // and because we dont know if building has started ( no good way for top states )
    // could change depth of top state to 1 then 0 would mean unitialized
    assert( !State_FinishedBuilding( current ) );

    // setup the parent info of new state
    if (current) {
        // what const cast are you speaking of?
        hsm_state_t* parent= (hsm_state_t*) &(current->desc);
            
        new_state->desc.parent= parent;
        new_state->desc.depth = parent->depth+1;

        // default init state for 'current' is the first child specified
        if (!parent->initial) {
            parent->initial= &(new_state->desc);
        }
    }

    // make new_state the new current by pushing onto builder stack
    // ( doesnt use machine's stack
    //   b/c we can have multiple nested hsmBegins()
    //   but we only have one "BuildingState" to exist in. )
    HsmContextPush( &(builder->stack), &(new_state->ctx) );

    // keep the builder context
    return status->ctx;
}

//---------------------------------------------------------------------------
static hsm_state BuildingStateEvent( hsm_status status )
{
    hsm_state ret= NULL;
    builder_t * builder= ((builder_t*)status->ctx);
    state_t* current= Builder_CurrentState( builder );
    switch (status->evt->type) 
    {
        case _hsm_enter: 
        {
            if (!current->desc.enter && status->evt->data.enter) {
                current->desc.enter= status->evt->data.enter;
                ret= HsmStateHandled();
            }
            else {
                if (current->desc.enter)  {
                    Builder_Error( builder, "enter already specified." );
                }
                else 
                if (!status->evt->data.enter) {
                    Builder_Error( builder, "enter is null." );
                }
                ret= HsmStateError();
            }
        }
        break; // a new event handler
        case _hsm_guard: 
        {
            ret= BuildingGuard();
        }
        break;
        case _hsm_end: {
            // an explict end finishes building the current state
            hsm_context ctx= HsmContextPop( &(builder->stack) );
            assert( ctx == &current->ctx && "we should have just popped current" );
            // pointing to 'self' is used as a key to indicate end has been called
            current->ctx.parent= &(current->ctx);
            // setup the event handler, this is another key that the state is good to go.
             current->desc.process= GenericEvent;
            // if this was the last matching begin/end pair, we're done
            if (builder->stack.count) {
                ret= HsmStateHandled();
            }
            else {
                ret= Building();
            }
        }            
        break;
    }
    return ret;
}

//---------------------------------------------------------------------------
// On: 
//---------------------------------------------------------------------------
// TODO: a very big deal for context is: what happens if you fail to allocate memory?
// how do you let the system know? if you wanted to be evil you lua-ize it; 
// and long jump right back to HsmProcessEvent. could also make it illegal to have a null context
// so returning null would signal an error.
static hsm_context BuildingGuardEnter( hsm_status status )
{
    builder_t * builder= ((builder_t*)status->ctx);
    GuardEvent * guard= (GuardEvent*)status->evt;
    state_t* state= Builder_CurrentState( builder );
    assert( state );
    assert( status->evt->type == _hsm_guard );
    NewHandler( state, status->evt->data.guard, guard->guard_data );
    // keep the original context
    return status->ctx;
}

//---------------------------------------------------------------------------
static hsm_state BuildingGuardEvent( hsm_status status )
{
    hsm_state ret=NULL;
    builder_t* builder= ((builder_t*)status->ctx);
    state_t* state= Builder_CurrentState( builder );
    assert(state);
    if (state) {
        handler_t* handler= state->handlers;

        switch (status->evt->type) {
            case _hsm_goto: {
                if (!handler->target && status->evt->data.go) {
                    handler->target= status->evt->data.go;
                    ret= HsmStateHandled();
                }
                else {
                    if (handler->target) {
                        Builder_Error( builder,"goto already specified for this event");
                    }
                    else 
                    if (!status->evt->data.go) {
                        Builder_Error( builder,"unknown target specified for goto");
                    }
                    ret= HsmStateError();
                }                
            }
            break;
            case _hsm_guard: {
                const GuardEvent* guard_event= (const GuardEvent*)status->evt;
                // on append, we want to add this guard to us
                // otherwise we want to start a new event handler
                if (guard_event->append) {
                    if (NewGuard( handler, guard_event->core.data.guard, guard_event->guard_data )) {
                        ret= HsmStateHandled();
                    }
                    else {
                        if (!guard_event->core.data.guard) {
                            Builder_Error( builder,"no guard specified");
                        }
                        else {
                            Builder_Error( builder,"couldnt allocate guard");
                        }
                        ret= HsmStateError();
                    }                    
                }
            }
            break;
            case _hsm_action: {
                const ActionEvent* action_event= (const ActionEvent*)status->evt;
                if (NewAction( handler, status->evt->data.run, action_event->action_data )) {
                    ret= HsmStateHandled();
                }
                else {
                    if (!status->evt->data.run) {
                        Builder_Error( builder,"no action specified for run");
                    }
                    else {
                        Builder_Error( builder,"couldnt allocate action for run");
                    }
                    ret= HsmStateError();
                }                
            }
            break;
        };        
    }        
    return ret;
}

//---------------------------------------------------------------------------
//
// Builder Interface:
//
//---------------------------------------------------------------------------

// there'd be one of each of these per thread context if such a thing were needed
static hsm_context_machine_t gMachine= {0};
static builder_t gBuilder= {0};
static int gStartCount=0;

//---------------------------------------------------------------------------
void hsmStartup()
{
    if (!gStartCount) {
        Hash_InitTable( &gBuilder.hash );
        HsmStart( HsmMachineWithContext( &gMachine, &(gBuilder.ctx) ), Building() );
    }
    ++gStartCount;
}

//---------------------------------------------------------------------------
void hsmShutdown()
{
    if (!--gStartCount) {
        const hsm_bool free_client_data= HSM_TRUE;
        Hash_DeleteTable( &gBuilder.hash, free_client_data );
    }        
}

//---------------------------------------------------------------------------
hsm_state hsmResolveId( int id ) 
{
    state_t* state= Builder_FindState( &gBuilder, id );
    // TODO: generic last error system for system
    return State_FinishedBuilding(state) ? &(state->desc) : NULL;
}

//---------------------------------------------------------------------------
hsm_state hsmResolve( const char * name ) 
{
    return hsmResolveId( hsmState( name ) );
}

//---------------------------------------------------------------------------
void hsmStartId( hsm_machine hsm, int id )
{
    state_t* state= Builder_FindState( &gBuilder, id );
    HsmStart( hsm, state ? &(state->desc) : HsmStateError() );
}

//---------------------------------------------------------------------------
void hsmStart( hsm_machine hsm, const char * name )
{
    hsmStartId( hsm, hsmState( name ) );
}

//---------------------------------------------------------------------------
// from the passed name, and the current state of the builder machine 
// we can know the named state's parent, we use this opportunity to create appropriate links.
// though, potentially, it could be save all the way till the very end
//
// this has to get more sophisticated
// if bare names ex. "Child" mean Parent::Child
// then how do we refer to plain parent (etc.) when we want it.
// for now we can assume they are always bare names
// i do, though

// TODO:
// unless the names get (consistantly) expanded fully to their complete size
// ( or split to their smallest size? ) it's going to be hard to compare
// but since collision is so painful, we might want to try, atleast in debug.

int hsmState( const char * name )
{
    int ret=0;
    hsm_uint32 id= CRC32( name );
    hash_entry_t* hash;
    state_t *current= Builder_CurrentState(&gBuilder);
    hash= Hash_CreateEntry( &gBuilder.hash, id, 0 );
    if (hash) {
        // in order to handle goto, we have to create the state desc but
        // in truth, we don't know much at all about the state.
        if (!hash->clientData) {
            hash->clientData= NewState( name, id );
        }
        ret= id;
    }
    return ret;

    if (hash) {
        ret= id;
    }
    return ret;
}

//---------------------------------------------------------------------------
// these dont trigger till complete, so need to look at how that works.
// doubtless, will have to copy off string, a pointer or offset to the memory that we have to fixup
// TODO: refs aren't completely necessary, though they are sometimes helpful. revisit?
#if 0
#if 0

// requires hsm_state

/*
enum hsmHints
{
    HSM_SIBLING,
    HSM_CHILD,
    HSM_SELF
};
*/
/**
 * Declare a new state.
 *
 * @param scope Scope of the passed name.
 * @param name Name relative to the scope
 *
 * @return int opaque handle to an internal name reference
 *
 * The name underlying the returned value is idempotent, but the returned value only does the best it can.
 * The value returned may not be the same as hsmState, even when they refer to the same state.
 * The variability of the returned value occurs because the scope may not even exist when the ref is declared.
 */
// int hsmRef( enum hsmHints scope, const char * name );
#endif

int hsmRef( enum hsmHints scope, const char * name )
{
    int ret=0;
    const char * scopes[] = {
        "SIBLING", "CHILD", "SELF"
    };
    const bool knownscope= (scope>=0 && (scope < (sizeof(scopes)/sizeof(*scopes))));
    assert( knownscope && "unknown scope requested" );
    if (knownscope) 
    {
        state_t * current= Builder_CurrentState(&gBuilder);
        assert( current && "scope only makes sense when there's context" );
        if (current) {
            hsm_uint32 id=  CRC32_CAT( name, CRC32_CAT( "!", current->id ) );
            hash_entry_t* hash= Hash_CreateEntry( &gBuilder.hash, id, 0 );
            if (hash) {
                // i don't want to build a state b/c it's so large
                // options: hash could hold an empty client data
                // we could expand the hash entry with a second client data
                // ( flags for instance )
                // we could have a double dereference through hash
                // ( ie. client data *always* holds a 'reference' structure -
                //   sometimes its a direct reference, sometimes indirect )
                // flags is the least wrk, even if a little evil
                hash->clientFlags|=1;
                hash->clientData= BuildReference( scope, name );
            }
        }            
    }        
    return ret;
}
#endif

//---------------------------------------------------------------------------
// all of the following functions generate handlers into the builder statemachine
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
int hsmBegin( const char * name )
{
    return hsmBeginId( hsmState( name ) );
}

//---------------------------------------------------------------------------
int hsmBeginId( int id )
{
    state_t* state= Builder_FindState( &gBuilder, id );
    BuilderEvent evt= { _hsm_begin, state };
    HsmProcessEvent( &gMachine.core, &evt );
    return id;
}

//---------------------------------------------------------------------------
void hsmOnEnter( hsm_callback_enter fn )
{
    BuilderEvent evt= { _hsm_enter };
    evt.data.enter= fn;
    HsmProcessEvent( &gMachine.core, &evt );
}

//---------------------------------------------------------------------------
void hsmOn( hsm_callback_guard guard, int guard_data )
{
    GuardEvent evt= { _hsm_guard }; 
    evt.core.data.guard= guard;
    evt.guard_data= guard_data;
    HsmProcessEvent( &gMachine.core, &(evt.core) );
}

//---------------------------------------------------------------------------
void hsmIf( hsm_callback_guard guard, int guard_data )
{
    GuardEvent evt= { _hsm_guard }; 
    evt.core.data.guard= guard;
    evt.guard_data= guard_data;
    evt.append= HSM_TRUE;
    HsmProcessEvent( &gMachine.core, &(evt.core) );
}

//---------------------------------------------------------------------------
void hsmGotoState( hsm_state state )
{
    BuilderEvent evt= { _hsm_goto };
    evt.data.go= state;
    HsmProcessEvent( &gMachine.core, &evt );
}

//---------------------------------------------------------------------------
void hsmGoto( const char * name )
{
    hsmGotoId( hsmState( name ) );
}

//---------------------------------------------------------------------------
void hsmGotoId( int id )
{
    state_t* state= Builder_FindState( &gBuilder, id );
    hsmGotoState( state ? &(state->desc) : 0 );
}

//---------------------------------------------------------------------------
void hsmRun( hsm_callback_action cb, int action_data )
{
    ActionEvent evt= { _hsm_action }; 
    evt.core.data.run= cb;
    evt.action_data= action_data;
    HsmProcessEvent( &gMachine.core, &evt.core );
}

//---------------------------------------------------------------------------
void hsmEnd()
{
    BuilderEvent evt= { _hsm_end };
    HsmProcessEvent( &gMachine.core, &evt );
}

