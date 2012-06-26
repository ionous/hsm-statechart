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
#include <string.h>

extern const hsm_uint32 HsmLowerTable[];

//---------------------------------------------------------------------------
// FNVa is in the public domain, http://isthe.com/chongo/tech/comp/fnv/
// http://dev.ionous.net/2007/03/string-hashes.html for more about string hashing
hsm_uint32 hsmStringHash(const char *string, hsm_uint32 hval) 
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
    hsm_state_t desc;       // the common statedescriptor

    hsm_callback_enter_ud enter;
    void * enter_ud;

    hsm_callback_action_ud exit;
    void * exit_ud;

    handler_t * handlers;   // list of events to handle
};

// querries for build status
#define Entry_ReadyToBuild( e )     ((e) && !(e)->clientData)
#define Entry_FinishedBuilding( e ) ((e) &&  (e)->clientData && (((hsm_state)(e)->clientData)->process== RunGenericEvent))
#define Entry_BuildInProgress( e )  ((e) &&  (e)->clientData && (((hsm_state)(e)->clientData)->process!= RunGenericEvent))


//---------------------------------------------------------------------------
/**
 * 
 */
struct action_rec
{
    int flags;               // tbd, might control how run is called
    hsm_callback_action_ud run; // the action is the same as 'handler', the caller can translate from there.
    void *action_data;         // passed to run
    action_t* next;           // we are a linked list, b/c there can be more than one action.
};


//---------------------------------------------------------------------------
/**
 * eventually want a full blown decision tree to allow ands and ors
 * right now, it's all ands
 */
struct guard_rec
{
    int flags;
    hsm_callback_guard_ud match;    // function to determine if this rec handles some any particular event
    void * guard_data;           // passed to match    
    guard_t * next;
};

//---------------------------------------------------------------------------
/**
 * event handler data.
 * one each event defined via the hsmBuilder interface.
 * may well be used for guards and events.
 *
 * re: target.
 *
 * when a user calls hsmGoto/Id the state in question might not yet exist:
 * so we can't store hsm_state directly; so instead we store the hash entry
 * ( could also store the id, but entry is safe, and fast, so why not. )
 *
 * now, its desired that hsmBuilder and core states interoperate
 * that's where hsmRegisterState comes from
 */
struct handler_rec
{
    guard_t guard;
    action_t *actions;      // if we do match: we may have things to do
    hash_entry_t* target;   //             not to mention, places to '.
    handler_t  *next;       // if not us, who? if not now, next?
};

/**
 * state_t extends hsm_status directly
 */
#define StateFromStatus( status ) ((state_t*) status->state)

//---------------------------------------------------------------------------
/**
 * run time helper to reflect singal event calls to the right guards and actions
 */
static hsm_context RunGenericEnter( hsm_status status )
{
    state_t* state= StateFromStatus( status );
    return state->enter( status, state->enter_ud );
}

//---------------------------------------------------------------------------
/**
 * run time helper to reflect singal event calls to the right guards and actions
 */
static void RunGenericExit( hsm_status status )
{   
    state_t* state= StateFromStatus( status );
    state->exit( status, state->exit_ud );
}

//---------------------------------------------------------------------------
/**
 * run time helper to reflect singal event calls to the right guards and actions
 */
static hsm_state RunGenericEvent( hsm_status status )
{
    hsm_state ret=NULL;
    state_t* state= StateFromStatus( status );
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
                ret= (hsm_state) et->target->clientData;
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
 * @param name State's name
 * @param namelen if non-zero, that number of name chars gets copied.
 */
static state_t* NewState( state_t* parent, const char * name, int namelen )
{   
    // note: calloc is keeping strncpy null terminated
    state_t* state=(state_t*) calloc( 1, sizeof( state_t ) + namelen + 1 );
    if (state) {
        if (!namelen) {
            state->desc.name= name;
        }
        else {
            // point the name to just have the block of size memory
            char* dest= (char*) ((size_t)state) + sizeof( state_t );
            assert( name && "namelen needs name" );
            if (name) {
                strncpy( dest, name, namelen );
                state->desc.name= dest;
            }                
        }

        // setup the parent info of new state
        if (parent) {
            state->desc.parent= &(parent->desc);
            state->desc.depth = parent->desc.depth+1;

            // default init state for 'parent' is the first child specified
            if (!parent->desc.initial) {
                parent->desc.initial= &(state->desc);
            }
        }
    }
    return state;
}

//---------------------------------------------------------------------------
/**
 * @internal construct a new event object
 */
static handler_t* NewHandler( state_t* state, hsm_callback_guard_ud match, void * guard_data )
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
static action_t* NewAction( handler_t* handler, hsm_callback_action_ud run, void * action_data )
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
static guard_t* NewGuard( handler_t* handler, hsm_callback_guard_ud match, void * guard_data )
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

/**
 * the handlers that the builder interface uses to build hsms.
 * ~= one per interface function.
 * why *shouldn't* a statemachine build a statemachine after all?
 */
enum builder_events 
{
    _hsm_begin,
    _hsm_end,

    _hsm_guard_ud,
    //_hsm_guard_raw,

    _hsm_enter_ud, 
    //_hsm_enter_raw,

    _hsm_exit_ud,
    //_hsm_exit_raw,

    _hsm_action_ud,
    //_hsm_action_raw,
    
    _hsm_goto,
    
};

/**
 * the builder interface is secretly backed by one of these.
 */
struct builder_rec
{
    hsm_context_t ctx;          // we are used as context in the builder machine
    const char *error;
    state_t * current;          // inner most state that's b/t begin,end.
    int count;                  // nested count of states
    hash_table_t hash;          // a hash of states
};

/**
 * builder_rec helper macro to return the state that's currently getting built
 */
#define Builder_CurrentState( b ) (state_t*)((b)->current)

/**
 * builder_rec helper macro to record an error string
 */
#define Builder_Error( b, e ) (b)->error=e

/**
 * 
 */
#define Builder_Valid( b ) ((b)->hash.bucketPtr !=0)

//---------------------------------------------------------------------------
/**
 * data associated with the builder_events_t
 * essentially: the interface functions repacked a function object.
 */
typedef struct hsm_event_rec BuildEvent; 
struct hsm_event_rec
{
    builder_events_t type;
    union _data {
        int id;
        void * data;
        hsm_callback_enter  raw_enter;
        hsm_callback_action raw_action;
        hsm_callback_guard  raw_guard;

    }
    data;
};

typedef struct begin_event_rec BeginEvent;
struct begin_event_rec
{
    BuildEvent core;
    const char * name;
    int namelen;
};

typedef struct enter_event_rec EnterEvent;
struct enter_event_rec
{
    BuildEvent core;
    hsm_callback_enter_ud enter;
    void * enter_data;   
};

typedef struct guard_event_rec GuardEvent;
struct guard_event_rec
{
    BuildEvent core;
    hsm_callback_guard_ud  guard;
    void * guard_data;   
    hsm_bool append;
};


typedef struct action_event_rec ActionEvent;
struct action_event_rec
{
    BuildEvent core;
    hsm_callback_action_ud  action;
    void * action_data;
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
        case _hsm_begin: 
        {
            hash_entry_t* entry= Hash_FindEntry( &builder->hash, status->evt->data.id );
            if (Entry_ReadyToBuild( entry )) {
                ret= BuildingState();
            }
            else {
                if (!entry) {
                    Builder_Error( builder, "hsmBegin for unknown state." );
                }
                else 
                if (Entry_FinishedBuilding( entry )) {
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
    BeginEvent * evt= (BeginEvent*)(status->evt);
    state_t* parent= Builder_CurrentState( builder );     // parent of the new state is the current state

    hash_entry_t* entry= Hash_CreateEntry( &(builder->hash), evt->core.data.id, 0 );
    state_t* new_state= NewState( parent, evt->name, evt->namelen );

    assert( new_state && entry->clientData == 0);
    if (new_state && entry->clientData == 0) 
    {
        entry->clientData= new_state;
        builder->current= new_state;      // later, we'll use the parent state to unwind
        ++builder->count;                 // 
    }        
    else {
        if (!new_state) {
            Builder_Error( builder, "couldn't allocate new state");
        }
        if (!entry->clientData) {
            Builder_Error( builder, "state data already exists");
        }
        else {
            Builder_Error( builder, "unknown error");
        }
    }

    // keep the builder as context
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
        case _hsm_enter_ud: {
            const EnterEvent* event= (const EnterEvent*)status->evt;
            if (!current->desc.enter && event->enter) {
                current->desc.enter= RunGenericEnter;
                current->enter= event->enter;
                current->enter_ud= event->enter_data;
                ret= HsmStateHandled();
            }
            else {
                Builder_Error( builder, current->desc.enter ? "enter already specified." : "enter is null." );
                ret= HsmStateError();
            }
        }
        case _hsm_exit_ud: {
            const ActionEvent* event= (const ActionEvent*)status->evt;
            if (!current->desc.exit && event->action) {
                current->desc.exit= RunGenericExit;
                current->exit= event->action;
                current->exit_ud= event->action_data;
                ret= HsmStateHandled();
            }
            else {
                Builder_Error( builder, current->desc.exit ? "exit already specified." : "exit is null." );
                ret= HsmStateError();
            }
        }
        break; // a new event handler
        case _hsm_guard_ud: {
            ret= BuildingGuard();
        }
        break;
        case _hsm_end: {
            // setup the event handler, this also a key that the state is good to go.
            current->desc.process= RunGenericEvent;

            // if this was the last matching begin/end pair, we're done
            // ( we dont use !current, b/c of potential containment in external states )
            if (--builder->count==0) {
                builder->current= 0;
                ret= Building();
            }
            else {
                builder->current= (state_t*) current->desc.parent;                    
                ret= HsmStateHandled();
            }
        }            
        break;
    }
    return ret;
}

//---------------------------------------------------------------------------
// On: 
//---------------------------------------------------------------------------
static hsm_context BuildingGuardEnter( hsm_status status )
{
    builder_t * builder= ((builder_t*)status->ctx);
    GuardEvent * guard= (GuardEvent*)status->evt;
    state_t* state= Builder_CurrentState( builder );
    assert( state );
    assert( status->evt->type == _hsm_guard_ud );
    NewHandler( state, guard->guard, guard->guard_data );
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
                const int go= status->evt->data.id;
                hash_entry_t* target= Hash_FindEntry( &(builder->hash), go );
                if (!handler->target && target) {
                    handler->target= target;
                    ret= HsmStateHandled();
                }
                else {
                    Builder_Error( builder, handler->target ? "goto already specified" : "unknown goto target" );
                    ret= HsmStateError();
                }                
            }
            break;
            case _hsm_guard_ud: {
                const GuardEvent* guard_event= (const GuardEvent*)status->evt;
                // on append, we want to add this guard to us
                // otherwise we want to start a new event handler
                if (guard_event->append) {
                    if (NewGuard( handler, guard_event->guard, guard_event->guard_data )) {
                        ret= HsmStateHandled();
                    }
                    else {
                        Builder_Error( builder, !guard_event->guard ? "no guard specified" : "couldnt allocate guard");
                        ret= HsmStateError();
                    }                    
                }
            }
            break;
            case _hsm_action_ud: {
                const ActionEvent* action_event= (const ActionEvent*)status->evt;
                if (NewAction( handler, action_event->action, action_event->action_data )) {
                    ret= HsmStateHandled();
                }
                else {
                    Builder_Error( builder, !action_event->action ? "no action specified" : "couldnt allocate action");
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
    if (gStartCount>0) {
        const hsm_bool free_client_data= HSM_TRUE;
        Hash_DeleteTable( &gBuilder.hash, free_client_data );
        --gStartCount;
    }        
}

//---------------------------------------------------------------------------
hsm_state hsmResolveId( int id ) 
{
    hsm_state ret=0;
    assert( gStartCount );
    if ( gStartCount && HsmIsRunning(&gBuilder) ) {
        const hash_entry_t* entry= Hash_FindEntry( &(gBuilder.hash), id );
        ret= (hsm_state) entry ? entry->clientData :  0;
    }        
    return ret;
}

//---------------------------------------------------------------------------
hsm_state hsmResolve( const char * name ) 
{
    return hsmResolveId( hsmState( name ) );
}

//---------------------------------------------------------------------------
hsm_bool hsmStartId( hsm_machine hsm, int id )
{
    return HsmStart( hsm, hsmResolveId( id ) );
}

//---------------------------------------------------------------------------
hsm_bool hsmStart( hsm_machine hsm, const char * name )
{
    return hsmStartId( hsm, hsmState( name ) );
}

//---------------------------------------------------------------------------
int hsmState( const char * name )
{
    int ret=0;
    assert( gStartCount );
    if ( gStartCount ) {
        hsm_uint32 id= HSM_HASH32( name );
        state_t *current= Builder_CurrentState(&gBuilder);
        /** 
         * note: i actually tried pre-allocating hsmState objects
         * and storing those in hsmGoto, but if the user code is using string names, 
         * the allocation has to know how/whether to copy the string;
         * that's not always obvious until hsmBegin()
         */
        hash_entry_t* hash= Hash_CreateEntry( &gBuilder.hash, id, 0 );
        if (hash) {
            ret= id;
        }
    }        
    return ret;
}

//---------------------------------------------------------------------------
// all of the following functions generate handlers into the builder statemachine
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
int hsmBegin( const char * name, int len )
{
    int ret=0;
    const int id= hsmState( name );
    if (id) {
        BeginEvent evt= { _hsm_begin, id };
        evt.name= name;
        evt.namelen= len;
        if (HsmSignalEvent( &gMachine.core, &evt.core )) {
            ret= id;
        }
    }        
    return id;
}

//---------------------------------------------------------------------------
int hsmBeginId( int id )
{
    int ret=0;
    assert( gStartCount );
    if ( gStartCount ) {
        BeginEvent evt= { _hsm_begin, id };
        if( HsmSignalEvent( &gMachine.core, &evt.core ) ) {
            ret=id;
        }            
    }        
    return ret;
}

//---------------------------------------------------------------------------
void hsmOnEnterUD( hsm_callback_enter_ud entry, void *enter_data )
{
    assert( gStartCount );
    if ( gStartCount ) {
        EnterEvent evt= { _hsm_enter_ud };
        evt.enter= entry;
        evt.enter_data= enter_data;
        HsmSignalEvent( &gMachine.core, &evt.core );
    }        
}

//---------------------------------------------------------------------------
void hsmOnExitUD( hsm_callback_action_ud action, void *exit_data )
{
    assert( gStartCount );
    if ( gStartCount ) {
        ActionEvent evt= { _hsm_exit_ud };
        evt.action= action;
        evt.action_data= exit_data;
        HsmSignalEvent( &gMachine.core, &evt.core );
    }        
}

//---------------------------------------------------------------------------
void hsmOnEventUD( hsm_callback_guard_ud guard, void *guard_data )
{
    assert( gStartCount );
    if ( gStartCount ) {
        GuardEvent evt= { _hsm_guard_ud }; 
        evt.guard= guard;
        evt.guard_data= guard_data;
        HsmSignalEvent( &gMachine.core, &(evt.core) );
    }        
}

//---------------------------------------------------------------------------
void hsmTestUD( hsm_callback_guard_ud guard, void *guard_data )
{
    assert( gStartCount );
    if ( gStartCount ) {
        GuardEvent evt= { _hsm_guard_ud }; 
        evt.guard= guard;
        evt.guard_data= guard_data;
        evt.append= HSM_TRUE;
        HsmSignalEvent( &gMachine.core, &(evt.core) );
    }        
}

//---------------------------------------------------------------------------
void hsmGoto( const char * name )
{
    hsmGotoId( hsmState( name ) );
}

//---------------------------------------------------------------------------
void hsmGotoId( int id )
{
    assert( gStartCount );
    if ( gStartCount ) {
        BuildEvent evt= { _hsm_goto, id };
        HsmSignalEvent( &gMachine.core, &evt );
    }        
}

//---------------------------------------------------------------------------
void hsmRunUD( hsm_callback_action_ud action, void *action_data )
{
    assert( gStartCount );
    if ( gStartCount ) {
         ActionEvent evt= { _hsm_action_ud }; 
        evt.action= action;
        evt.action_data= action_data;
        HsmSignalEvent( &gMachine.core, &evt.core );
    }        
}

//---------------------------------------------------------------------------
/*
void hsmRun( hsm_callback_action cb, void *action_data )
{
    ActionEvent evt= { _hsm_action_raw }; 
    evt.core.action= cb;
    HsmSignalEvent( &gMachine.core, &evt.core );
}
*/

//---------------------------------------------------------------------------
void hsmEnd()
{
    assert( gStartCount );
    if ( gStartCount ) {
        BuildEvent evt= { _hsm_end };
        HsmSignalEvent( &gMachine.core, &evt );
    }        
}
