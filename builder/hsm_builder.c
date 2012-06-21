#include <hsm/hsm_machine.h>
#include "hash.h"
#include "hsm_builder.h"
#include <assert.h>
#include <stdlib.h>

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
typedef struct build_state_rec  build_state_t;
typedef struct build_event_rec  build_event_t;
typedef struct build_action_rec build_action_t;
typedef hsm_bool (*hsm_match_event)( hsm_machine hsm, hsm_context ctx, hsm_event evt, void * user_data );


//---------------------------------------------------------------------------
/**
 * one each state defined via the hsmBuilder interface.
 * it augments the 'common' state descriptor.
 * with data to generically handle events, guards, actions, and transitions.
 */
struct build_state_rec
{
    hsm_context_t ctx;          // we push them on hsmBegin, pop them on hsmEnd
                                // it's reused to indicate build completion status
                                // null => hasnt been the subject of an hsmBegin
                                // self => finished hsmEnd
                                // anything else => in progress
    hsm_state_t desc;           // the common statedescriptor
    hsm_uint32  id;             // unique id for searching for this state
    build_event_t * events;     // list of events to handle
};

//---------------------------------------------------------------------------
/**
 * 
 */
struct build_action_rec
{
    hsm_callback_action run; // the action is the same as 'process', the caller can translate from there.
    build_action_t* next;           // we are a linked list, b/c there can be more than one action.
};

//---------------------------------------------------------------------------
/**
 * one each event defined via the hsmBuilder interface.
 * may well be used for guards and events.
 */
struct build_event_rec
{
    // todo: chain these so we can filter guards?
    hsm_match_event match;        // function to determine if this rec handles some any particular event
    void * user_data;             // passed to match

    //
    build_action_t *actions;      // if we do match: we may have things to do
    hsm_state        target;      //             not to mention, places to go.
    build_event_t  *next;         // if not us, who? if not now, next?
};


//---------------------------------------------------------------------------
/**
 * run time helper used via build_event_t
 * assumes the user data is an int value ( like an enum would be )
 */
static hsm_bool HsbMatchInt( hsm_machine hsm, hsm_context ctx, hsm_event evt, void * user_data )
{
    return *((int*)(evt))== ((int)(user_data));
}

/**
 * run time helper used via build_event_t
 * assumes the user data is a function ( like a guard would be )
 */
static hsm_bool HsbMatchGuard( hsm_machine hsm, hsm_context ctx, hsm_event evt, void * user_data )
{
    hsm_callback_guard guard= (hsm_callback_guard)user_data;
    return guard( hsm, ctx, evt );
}

/**
 * run time helper used via build_state_t
 */
static hsm_state HsbProcessEvent( hsm_machine hsm, hsm_context ctx, hsm_event evt )
{
    // TODO:
    // extract the builder for the state
    // most likely though very evil pointer math

    // then match its list of events
    // run its actions
    // etc.
    
    return NULL;
}



//---------------------------------------------------------------------------
/**
 * internal: construct a new state object
 */
static build_state_t* BuildState( hsm_state parent, const char * name, hsm_uint32 id )
{   
    build_state_t* state=(build_state_t*) calloc( sizeof( build_state_t ),0 );
    if (state) {
        state->id= id;
        state->desc.name= name;
        state->desc.parent= parent;
        state->desc.process= HsbProcessEvent;
        state->desc.depth = parent? parent->depth+1 : 0;
    }
    return state;
}

//---------------------------------------------------------------------------
/**
 * internal: construct a new action object
 */
static build_action_t* BuildAction( build_event_t* event, hsm_callback_action run )
{
    build_action_t* action= (build_action_t*) calloc( sizeof( build_action_t ), 0 );
    if (action) {
        // set:
        action->run= run;
        //link:
        action->next= event->actions;
        event->actions= action;
    }
    return action;
}

//---------------------------------------------------------------------------
/**
 * internal: construct a new event object
 */
static build_event_t* BuildEvent( build_state_t* state, hsm_match_event match, void * user_data )
{   
    build_event_t* event= (build_event_t*) calloc( sizeof( build_event_t ),0 );
    if (event) {
        event->match= match;
        event->user_data= user_data;
        event->next= state->events;
        state->events= event;
    }        
    return event;
}

//---------------------------------------------------------------------------
// Builder Machine
//---------------------------------------------------------------------------

typedef struct builder_rec builder_t;
typedef enum builder_events builder_events_t;
typedef struct hsm_event_rec hsm_event_t;

/**
 * the events that the builder interface uses to build hsms.
 * ~= one per interface function.
 * why *shouldn't* a statemachine build a statemachine after all?
 */
enum builder_events {
    _hsm_begin,
    _hsm_end,
    _hsm_on,
    _hsm_enter,
    _hsm_goto,
    _hsm_run,
    
    // _hsm_if
    // _hsm_exit,
};

/**
 * the builder interface is secretly backed by one of these.
 */
struct builder_rec
{
    hsm_context_t ctx;
    const char *error;
    hsm_context_stack_t stack;  // a stack of processing states.
    hash_table_t hash;          // a hash of states
};


/**
 * builder_rec helper macro to return the state that's currently getting built
 */
#define Builder_CurrentState( b ) (build_state_t*)((b)->stack.context)

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
        build_state_t * state;
        hsm_callback_enter enter;
        //hsm_callback_exit exit;
        hsm_callback_action run;
        hsm_state go;
        int on_int;
    }
    data;
};


HSM_STATE( Building, HsmTopState, BuildingIdle );
    HSM_STATE( BuildingIdle, Building, 0 );
    HSM_STATE_ENTER( BuildingState, Building, BuildingBody );
        HSM_STATE( BuildingBody, BuildingState, 0 );
        HSM_STATE_ENTER( BuildingOn, BuildingState, 0 );


//---------------------------------------------------------------------------
// Build:
//---------------------------------------------------------------------------
static hsm_state BuildingEvent( hsm_machine hsm, hsm_context ctx, hsm_event evt )
{
    hsm_state ret= HsmStateError(); // at the top level, by default, everythings an error.
    builder_t * builder= ((builder_t*)ctx);
    switch (evt->type) {
        case _hsm_begin: {
            build_state_t * state=evt->data.state;
            if (state && !state->ctx.parent) {
                ret= BuildingState();
            }
            else {
                if (!state) {
                    Builder_Error( builder, "hsmBegin for unknown state." );
                }
                else 
                if (state->ctx.parent == &(state->ctx)) {
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
static hsm_state BuildingIdleEvent( hsm_machine hsm, hsm_context ctx, hsm_event evt )
{
    return NULL;  
}

//---------------------------------------------------------------------------
// State:
//---------------------------------------------------------------------------
static hsm_state BuildingBodyEvent( hsm_machine hsm, hsm_context ctx, hsm_event evt )
{   
    return NULL;
}

//---------------------------------------------------------------------------
static hsm_context BuildingStateEnter( hsm_machine hsm, hsm_context ctx, hsm_event evt )
{
    builder_t * builder= ((builder_t*)ctx);
    // we push onto the builder's stack, not the machine's stack
    // b/c we can have multiple nested hsmBegins()
    // but we only have one "BuildingState" to exist in.
    build_state_t*state= evt->data.state;
    assert( evt->type == _hsm_begin );

    // default initializer:
    if (state->desc.parent && !state->desc.parent->initial) {
        //
        // TODO: FIX: TROUBLE initial is a parameterless function, desc is an object.
        // most likely: you'll have to do switch the fn to an object if you can.

        // state->desc.parent->initial= &(state->desc);
    }
    
    HsmContextPush( &(builder->stack), &(state->ctx) );
    // keep the builder context
    return ctx;
}

//---------------------------------------------------------------------------
static hsm_state BuildingStateEvent( hsm_machine hsm, hsm_context ctx, hsm_event evt )
{
    hsm_state ret= NULL;
    builder_t * builder= ((builder_t*)ctx);
    build_state_t* state= ((build_state_t*)builder->stack.context);
    switch (evt->type) 
    {
        case _hsm_enter: 
        {
            if (!state->desc.enter && evt->data.enter) {
                state->desc.enter= evt->data.enter;
                ret= HsmStateHandled();
            }
            else {
                if (state->desc.enter)  {
                    Builder_Error( builder, "enter already specified." );
                }
                else 
                if (!evt->data.enter) {
                    Builder_Error( builder, "enter is null." );
                }
                ret= HsmStateError();
            }
        }
        break;
        case _hsm_on: 
        {
            ret= BuildingOn();
        }
        break;        
        case _hsm_end: {
            // an explict end finishes building the current state:
            HsmContextPop( &(builder->stack) );
            // 'self' used as a key to indicate end has been called:
            state->ctx.parent= &(state->ctx);
            // if this was the last matching begin/end pair, we're done:
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
static hsm_context BuildingOnEnter( hsm_machine hsm, hsm_context ctx, hsm_event evt )
{
    builder_t * builder= ((builder_t*)ctx);
    // TODO: a very big deal for context is: what happens if you fail to allocate memory?
    // how do you let the system know?
    build_state_t* state= Builder_CurrentState( builder );
    BuildEvent( state, HsbMatchInt, (void*) evt->data.on_int );
    // keep the original context
    return ctx;
}

//---------------------------------------------------------------------------
static hsm_state BuildingOnEvent( hsm_machine hsm, hsm_context ctx, hsm_event evt )
{
    hsm_state ret=NULL;
    builder_t* builder= ((builder_t*)ctx);
    build_state_t* state= Builder_CurrentState( builder );
    build_event_t* event= state->events;

    switch (evt->type) {
        case _hsm_goto: {
            // we can't lookup the actual states until 'complete'
            // all we can do is record the structure for later
            if (!event->target && evt->data.go) {
                event->target= evt->data.go;
                ret= HsmStateHandled();
            }
            else {
                if (event->target) {
                    Builder_Error( builder,"goto already specified for this event");
                }
                else 
                if (!evt->data.go) {
                    Builder_Error( builder,"null target specified for goto");
                }
                ret= HsmStateError();
            }                
        }
        break;
        case _hsm_run: {
            build_action_t* action= BuildAction( event, evt->data.run );
            if (action && action->run) {
                ret= HsmStateHandled();
            }
            else {
                if (!evt->data.run) {
                    Builder_Error( builder,"null action specified for run");
                }
                else 
                if (!action) {
                    Builder_Error( builder,"couldnt allocate action for run");
                }
                ret= HsmStateError();
            }                
        }
        break;
    };        
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

//---------------------------------------------------------------------------
void hsmStartup()
{
    HsmMachineWithContext( &gMachine, &(gBuilder.ctx) );
    Hash_InitTable( &gBuilder.hash );
}

//---------------------------------------------------------------------------
void hsmShutdown()
{
    hsm_bool free_client_data= HSM_TRUE;
    Hash_DeleteTable( &gBuilder.hash, free_client_data );
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
int hsmState( const char * name )
{
    int ret=0;
    hsm_uint32 id;    
    hsm_state parent;
    hash_entry_t* hash;
    build_state_t *current= Builder_CurrentState(&gBuilder);

    if (!current) {
        id= CRC32( name );
        parent= NULL;
    }
    else {
        id= CRC32_CAT( name, CRC32_CAT( "::", current->id ) );
        parent= &(current->desc);
    }

    hash= Hash_CreateEntry( &gBuilder.hash, id, 0 );
    if (hash) {
        // TODO:
        // unless the names get (consistantly) expanded fully to their complete size
        // ( or split to their smallest size? ) it's going to be hard to compare
        // but since collision is so painful, we might want to try, atleast in debug.
        if (!hash->clientData) {
            hash->clientData= BuildState( parent, name, id );
        }
        ret= id;
    }
    return ret;
}

//---------------------------------------------------------------------------
// these dont trigger till complete, so need to look at how that works.
// doubtless, will have to copy off string, a pointer or offset to the memory that we have to fixup
// TODO: refs aren't completely necessary, though they are sometimes helpful. revisit?
#if 0
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
        build_state_t * current= Builder_CurrentState(&gBuilder);
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
// all of the following functions generate events into the builder statemachine
//---------------------------------------------------------------------------
void hsmOni( int i )
{
    hsm_event_t evt= { _hsm_on }; evt.data.on_int=i;
    HsmProcessEvent( &gMachine.core, &evt );
}

//---------------------------------------------------------------------------
void hsmGoto( int id )
{
    hash_entry_t* entry= Hash_FindEntry( &(gBuilder.hash), id );
    build_state_t* state= entry ? (build_state_t*) entry->clientData : NULL;
    hsm_event_t evt= { _hsm_goto, state };
    HsmProcessEvent( &gMachine.core, &evt );
}

//---------------------------------------------------------------------------
void hsmRun( hsm_callback_action cb )
{
    hsm_event_t evt= { _hsm_run }; 
    evt.data.run= cb;
    HsmProcessEvent( &gMachine.core, &evt );
}

//---------------------------------------------------------------------------
void hsmOnEnter( hsm_callback_enter fn )
{
    hsm_event_t evt= { _hsm_enter };
    evt.data.enter= fn;
    HsmProcessEvent( &gMachine.core, &evt );
}

//---------------------------------------------------------------------------
int hsmBegin( int id )
{
    hash_entry_t* entry= Hash_FindEntry( &(gBuilder.hash), id );
    build_state_t* state= entry ? (build_state_t*) entry->clientData : NULL;
    hsm_event_t evt= { _hsm_begin, state };
    HsmProcessEvent( &gMachine.core, &evt );
    return id;
}

//---------------------------------------------------------------------------
void hsmEnd()
{
    hsm_event_t evt= { _hsm_end };
    HsmProcessEvent( &gMachine.core, &evt );
}

