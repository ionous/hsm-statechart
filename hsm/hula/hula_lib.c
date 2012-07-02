#include <hsm/hsm_machine.h>
#include <lua.h>
#include <lauxlib.h>

#include "hula.h" // for build during new.
#include "hula_lib.h"
#include "hula_types.h"
#include <hsm/builder/hsm_builder.h> // resolve needed for looking up top state names during new

#include <assert.h>
#include <string.h>

#define HULA_REC_IDX 1
#define HULA_EVENT_IDX 2
#define HULA_PAYLOAD_IDX 3
#define HULA_CHART_IDX 2

//---------------------------------------------------------------------------
/**
 * @internal 
 * @return value hula hsm object
 */
static hula_machine_t* hula_check( lua_State* L, int idx )
{
    void * ud= luaL_checkudata( L, idx, HULA_METATABLE );
    luaL_argcheck( L, ud != NULL, idx, "hsm type expected");
    return (hula_machine_t*) ud;
}

//---------------------------------------------------------------------------
/**
 * @internal 
 * create a new table and fill it with a list of items from the stack
 * expected: event name, event parameters...
 * @return index of new table
 */
static int hula_pack( lua_State * L, int copyfrom, int copyto )
{
    int count=0; 
    int copy=copyfrom;
    lua_newtable( L );
    while( copy<=copyto ) {
        lua_pushvalue( L, copy++ );
        lua_rawseti( L, -2, ++count );
    }
    return count;
}

//---------------------------------------------------------------------------
/**
 * Create a new hsm machine.
 *
 * machine= hsm.new{ chart, init= 'name of first state', context= data }
 *
 * Calling hsm.new with the same table twice is currently undefined.
 *
 * @see HsmStart, HsmMachineWithContext 
 */
static int hsm_new(lua_State* L)
{
    const int table= lua_gettop(L);
    if (!lua_istable(L,table)) {
        lua_pushstring( L, "expected a table" );
        lua_error( L );
    }
    else {
        int id=0;
        // get the chart: chart can be either user data, or a machine to clone
        lua_rawgeti( L, table, 1 );
        if(lua_isuserdata( L, -1 )) {
            hula_machine_t* src= hula_check( L, HULA_REC_IDX );
            if (src) {
                id= src->topstate;
            }            
            lua_pop( L, 1 );
        }
        // or a state chart declarative table
        else {
            hula_error err= HulaBuildState( L, HULA_CHART_IDX, &id );
            if (err) {
                lua_pushstring( L,err );
                lua_error(L);
            }    
        }
        lua_pop( L,1 ); // pop the chart

        if (id) {
            // get init(ial) state
            hsm_state init_state;
            lua_getfield( L, table, "init" );
            if (lua_isstring( L, -1)) {
                const char * statename=lua_tostring( L, -1 );
                init_state=hsmResolve( statename );
            }
            else {
                init_state= hsmResolveId( id );
            }
            lua_pop( L, 1 ); // pop init

            if (!init_state) {
                lua_pushstring( L, "resolve error during init");
                lua_error(L);
            }
            else {
                hula_machine_t* hula;
                // get context:
                int ctx;
                lua_getfield( L, table, "context" );
                ctx= luaL_ref(L, LUA_REGISTRYINDEX);
                
                // user data becomes return value
                hula= (hula_machine_t*) lua_newuserdata( L, sizeof(hula_machine_t) );
                assert( hula );
                if (hula) {
                    memset( hula, 0, sizeof(hula_machine_t) );
                    hula->topstate= id;
                    hula->ctx.L= L;
                    hula->ctx.lua_ref= ctx;                    
                    luaL_getmetatable( L, HULA_METATABLE );
                    lua_setmetatable( L, -2 );

                    if (HsmMachineWithContext( &hula->hsm, &hula->ctx.core )) {
                        hula->hsm.core.flags|= HSM_FLAGS_HULA;
                        
                        // create the event table with "init" as the event name
                        lua_newtable( L );
                        lua_pushstring( L,"init" );
                        lua_rawseti( L, -2, 1 );
                        
                        // start the machine
                        if (HsmStart( (hsm_machine) &hula->hsm, init_state )) {
                            lua_pop( L, 1 ); // remove the event table
                        }
                        else {
                            lua_pop( L, 1 ); // remove the event table
                            lua_pushstring( L, "couldnt start machine");
                            lua_error( L );
                        }
                    }                        
                }                    
            }            
        }
    }
    return 1; 
}

/**
 * Send an event to the statemachine.
 * boolean= hsm.signal( event, payload )
 * @see HsmSignalEvent
 */
static int hsm_signal(lua_State* L)
{
    hsm_bool okay= HSM_FALSE;
    hula_machine_t* hula= hula_check(L, HULA_REC_IDX);
    if (hula) {
        const char * event_name= luaL_checkstring(L, HULA_EVENT_IDX);
        if (event_name) {
            hula_pack( L, HULA_EVENT_IDX, lua_gettop(L) );
            okay= HsmSignalEvent( (hsm_machine) &hula->hsm, 0 );
        }
    }        
    lua_pushboolean( L, okay );
    return 1;
}

/**
 * Return a complete listing of the machine's current states.
 * table= hsm.get_states()
 * @see HsmIsInState
 */
static int hsm_states(lua_State *L)
{
    int count=0;
    hula_machine_t* hula= hula_check(L,HULA_REC_IDX);
    if (hula) {
        hsm_state state;
        for ( state= hula->hsm.core.current; state; state=state->parent, ++count ) {
            lua_pushstring( L, state->name );
        }
    }        
    return count;
}

/**
 * Determine whether the state machine has finished.
 * boolean= hsm.is_running() 
 * 
 * @return true if the machine is not in a final or error state
 * @see HsmIsRunning
 */
static int hsm_is_running(lua_State *L)
{
    hsm_bool okay= HSM_FALSE;
    hula_machine_t* hula= hula_check(L,HULA_REC_IDX);
    if (hula) {
        okay= HsmIsRunning( (hsm_machine) &hula->hsm );
    }        
    lua_pushboolean( L, okay );
    return 1;
}

/**
 * Print the hsm in a handy debug form.
 * string= hsm.__tostring()
 * 
 * @return "hsm(topState:leafState)
 * @see hula_states
 */
static int hsm_tostring(lua_State *L)
{
    int ret=0;
    hula_machine_t* hula= hula_check(L,HULA_REC_IDX);
    if (hula) {
        // hrmm... breaking into the lower level api....
        hsm_state topstate= hsmResolveId( hula->topstate );
        lua_pushfstring(L, "hsm_statechart: %s:%s", 
                        topstate ? topstate->name : "nil",
                        hula->hsm.core.current ? hula->hsm.core.current->name : "nil" );
        ret=1;                                      
    }        
    return ret;
}

//---------------------------------------------------------------------------
// Registration
//---------------------------------------------------------------------------

void HulaNamedRegister( lua_State* L, const char * type, hula_callback_is_event is_event )
{
    static luaL_Reg hsm_class_fun[]= {
        { "new", hsm_new },
        { 0 }
    };

    static luaL_Reg hsm_member_fun[]= {
        // is_event
        { "__tostring", hsm_tostring },
        { "signal", hsm_signal },
        { "states", hsm_states },
        { "is_running", hsm_is_running },
        { 0 }
    };

    // an object with metatable, when it sees it doesn't have t[k] uses __index to see what to do
    // __index could have a function, value, or table; 
    // tables say: here's a table of names and functions for you. 
    // all meta methods start with __, http://lua-users.org/wiki/MetatableEvents
    luaL_newmetatable( L, HULA_METATABLE );  // registry.HULA_METATABLE= {}
    
    lua_pushstring( L, HULA_EVENT_TEST );
    lua_pushlightuserdata( L, is_event );
    lua_settable( L, -3 );

    lua_pushstring( L, "__index" ); 
    lua_pushvalue( L, -2 );  // copy the metatable to the top of the stack
    lua_settable( L, -3 );  // metatable.__index== metatable;
    
    luaL_register( L, NULL, hsm_member_fun );  // use the table @ top (metatable), and assign the passed named c-functions
    luaL_register( L, type, hsm_class_fun );   // create a table in the registry @ 'type' with the passed named c-functions
    lua_pop(L,2);
}

//---------------------------------------------------------------------------
void HulaRegister( lua_State* L, hula_callback_is_event is_event  )
{
    HulaNamedRegister( L, HULA_LIB, is_event );
}
