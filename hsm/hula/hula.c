/**
 * @file hula.c
 *
 * \internal
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include <hsm/hsm_machine.h>
#include <hsm/builder/hsm_builder.h>
#include "hula.h"


#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <assert.h>
#include <string.h>

#include "C:\dev\lua-5.1.4\src\lstate.h"


const char * HULA_ERR_UNKNOWN= "HULA_ERR_UNKNOWN";
// invalid arg passed to function
const char * HULA_ERR_ARG= "HULA_ERR_ARG"; 
// keys should be strings
const char * HULA_ERR_UNEXPECTED_KEY= "HULA_ERR_UNEXPECTED_KEY"; 
// values should be strings, tables, or functions
const char * HULA_ERR_UNEXPECTED_VALUE= "HULA_ERR_UNEXPECTED_VALUE"; 

//---------------------------------------------------------------------------
// Lua helpers 
//---------------------------------------------------------------------------
typedef struct nstring_rec nstring_t;

/**
 * @internal
 * lua helper for strings with length
 */
struct nstring_rec 
{
    const char * string;
    size_t len;
};

#define NSTRING(l) ((l).string && (l).len)
#define NSTRING_IS(l, s) (strcmp((l).string,s)==0)

#define lua_tonstring( L, idx, nstring ) (nstring)->string= lua_tolstring( L, idx, &((nstring)->len) )
#define lua_pushnstring( L, nstring ) lua_pushlstring( L, (nstring).string, (nstring).len )

//---------------------------------------------------------------------------
// State Tables
//---------------------------------------------------------------------------

/**
 * @internal
 * State Tables store per state lua data, mainly:
 * the "address" of lua function callbacks, but also state context data
 */

// raw entries on the state table
#define LUA_T_ENTER 0
#define LUA_T_EXIT  1
#define LUA_T_CTX   2

/**
 * get our internal table of state tables; creates it if it doesnt exist.
 * we use our own table, rather than just the registry itself as a "namespace"
 * to protect our state tables from collisions with other libraries
 */
static int HulaGetStateTables( lua_State * L )
{
    static int tablespot=0;
    lua_pushlightuserdata( L, &tablespot );  // unique spot for tables in the registry
    lua_gettable( L, LUA_REGISTRYINDEX );    // pull registry[tablespot]

    // is this the first time we're using registry[tablespot]?
    if (lua_isnil( L, -1 )) {
        lua_pop(L,1);
        // create a new table
        lua_pushlightuserdata( L, &tablespot );   // key,
        lua_newtable( L );                        // value,
        // store it in the registry
        lua_settable( L, LUA_REGISTRYINDEX );     // registry[key]=value
        // pull it back from the registry
        lua_pushlightuserdata( L, &tablespot ); 
        lua_gettable( L, LUA_REGISTRYINDEX );
    }
    return lua_gettop( L );
}

/**
 * pull the state's table to the stack
 * it would be cool to be able to use the hsm_state object itself.
 * it would need special acccess to the builder to access the in progress state though
 * and that's not very nice, so we just use name instead
 */
static int HulaGetStateTable( lua_State * L, const char * statename )
{
    const int tables= HulaGetStateTables( L ); // pull tables onto the stack
    lua_getfield( L, tables, statename );    // get tables[ state->name ]
    lua_remove( L, tables );                   // remove the tables
    return lua_gettop(L);                       // return statetable
}

/**
 * @internal
 * create a new state table
 * the table is stored in the registry: registry[ tables [ statename ]  ]
 */
static int HulaCreateStateTable( lua_State * L, nstring_t statename )
{
    const int check= lua_gettop(L);
    const int tables= HulaGetStateTables( L );  // pull tables onto the stack
    lua_pushnstring( L, statename );            // key=statename
    lua_newtable(L);                            // value={}
    lua_settable( L, tables );                  // tables[key]=value
    lua_remove( L, tables );                    // remove the tables
    assert( check == lua_gettop(L) );
    return HulaGetStateTable( L, statename.string );
}

//---------------------------------------------------------------------------
/**
 * @internal
 */
static hsm_context HulaEnter( hsm_status status, void * user_data )
{
    lua_State* L= (lua_State*)user_data;
    int err;
    const int state_table= HulaGetStateTable( L, status->state->name );
    int args=0;

    // push parameters:
    // ++args;
    // push the function
    lua_rawgeti( L, state_table, LUA_T_ENTER ); 
    // call the function
    err= lua_pcall(L, args,1,0); //? TODO: and do what on error exactly?
    // store the returned value as context
    lua_rawseti( L, state_table, LUA_T_CTX );
    // pop the state table
    lua_remove(L,state_table);

    // return our old context
    return status->ctx;
}

//---------------------------------------------------------------------------
/**
 *  @internal
 */
static void HulaExit( hsm_status status, void * user_data )
{
    lua_State* L= (lua_State*)user_data;
    int err;
    const int state_table= HulaGetStateTable( L, status->state->name );
    int args=0;
    
    // push paramaters
    lua_rawgeti( L, state_table, LUA_T_CTX ); ++args;
    // push the function
    lua_rawgeti( L, state_table, LUA_T_EXIT );
    // call the function
    err= lua_pcall(L, args, 0, 0); //? TODO: and do what on error exactly?
    // release the old context data
    lua_pushnil( L ); lua_rawseti( L, state_table, LUA_T_CTX );
    // pop the state table
    lua_remove(L,state_table);
}

//---------------------------------------------------------------------------
/**
 * @internal 
 */
static hsm_state HulaRun( hsm_status status, void * user_data )
{
    hsm_state ret=0;
    lua_State* L= (lua_State*)user_data;
    int table= HulaGetStateTable( L, status->state->name );
    int args=0;

    // run the function
    int err= lua_pcall(L, args, 1, 0); 
    if (err) {
        err= HsmStateError();
    }
    else {
        // evaluate the results
        if (lua_isstring( L, -1 )) {
            const char * name= lua_tostring( L, -1 );
            ret= hsmResolve( name );
        }
        else
        if (lua_isboolean( L, -1 ) && lua_toboolean( L, -1 )) {
            ret= HsmStateHandled();
        }

        // pop the state table and function results
        lua_pop(L,2);
    }
    // return the next state
    return ret;
}


//---------------------------------------------------------------------------
/**
 * 
 */
const char * INIT  = "init";
const char * ENTRY = "entry";
const char * EXIT  = "exit";
 
/**
 * @internal
 * lua sure does make for long functions.
 * expects table is @-1, 
 * we're going to assume its filled with states and stuff
 */
static hula_error _HulaBuildState( lua_State*L, const int table, nstring_t statename ) 
{
    hula_error err= 0;
    
    if (!NSTRING(statename)) {
        err= HULA_ERR_ARG;
    }
    else {
        int initState=0;
        // check for an 'init' entry
        // ie. the right side of { -init='s1' }
        lua_getfield( L, table, INIT );  
        // its okay not to have an init; 
        // ( TODO: a check to make the state deosnt have children? )
        if (lua_isnil( L, -1 )) {
            lua_pop(L,1); 
        }
        else 
        if (!lua_isstring( L, -1) ) {
            err= HULA_ERR_UNEXPECTED_KEY;
            lua_pop(L,1);
        }
        else {
            nstring_t initname;
            lua_pushvalue( L, -1 );                // dupe the name: we need to keep a pointer to it
            lua_tonstring( L, -2, &initname );
            // from the main, get table the definition of the init state, 
            // ie. the right side of { 's1'= {} }
            lua_gettable( L, table );              
            if (!(initState= hsmBegin( initname.string, initname.len ))) {
                err= HULA_ERR_UNKNOWN;
            }
            else {
                const int check= lua_gettop(L);
                err= _HulaBuildState( L, lua_gettop(L), initname );
                hsmEnd();
                assert( check == lua_gettop(L) );
            }            
            lua_pop( L, 2 ); // pop the init dupe, and the init state's table.
        }

        if (!err) {
            // create a lua table to hold functions
            const int state_table= HulaCreateStateTable( L, statename );
            // walk the contents of the source table
            lua_pushnil(L);
            while (lua_next(L, table)) {
                nstring_t keyname; 
                const int value_idx= lua_gettop(L);
                const int key_idx = value_idx-1;
                                
                // we expect that key is always a string. not just stringable.
                // (  note: in place coercion of non-string key can corrupt lua_next() )
                if (err || !lua_isstring( L, key_idx )) {
                    err= err ? err : HULA_ERR_UNEXPECTED_KEY; 
                    lua_pop(L,2);  // pop loop iterators
                    break; 
                }
                
                lua_tonstring( L, key_idx, &keyname );
                // if the key:value is a a table, its a state, we need to create it.
                if ( lua_istable( L, value_idx ) )  {
                    // but: if the nextState is our initial state... 
                    // then we've already created it. ( i wish there was a simpler way )
                    const int nextState= hsmState( keyname.string );
                    if (nextState!= initState) {
                        if (!hsmBegin( keyname.string, keyname.len )) {
                            err= HULA_ERR_UNKNOWN;
                        }
                        else {
                            const int check= lua_gettop(L);
                            err= _HulaBuildState( L, value_idx, keyname );
                            assert( check== lua_gettop(L) );
                            hsmEnd(); 
                        }
                    }                        
                    assert( value_idx == lua_gettop(L) );
                    lua_pop(L,1); // pop `value`, leaving key on top for lua_next() loop
                }
                else  {
                    const int is_target_function= lua_isfunction( L, value_idx );
                    const int is_target_name= lua_isstring( L, value_idx );
                    if (!is_target_name && !is_target_function) {
                        err= HULA_ERR_UNEXPECTED_VALUE;
                        lua_pop(L,2); // pop loop iterators
                        break;
                    }
                    else {
                        // Init: ex. { init = 's11' } but .... we took care of it already.
                        if (NSTRING_IS( keyname, INIT )) {
                            lua_pop(L,1); // manually pop the value
                        }
                        else 
                        // Entry: ex. { enter = function() end }
                        if (is_target_function && NSTRING_IS( keyname, ENTRY )) {
                            // state_table['exit']= function
                            lua_rawseti( L, state_table, LUA_T_ENTER ); // value is popped.
                            hsmOnEnterUD( HulaEnter, L );
                        }
                        else 
                        // Exit: ex. { enter = function() end }
                        if (is_target_function && NSTRING_IS( keyname, EXIT )) {
                            // state_table['exit']= function
                            lua_rawseti( L, state_table, LUA_T_EXIT ); // value is popped.
                            hsmOnExitUD( HulaExit, L );
                        }
                        // Event: ex. { event = 'name' }, or: { event = function() end }
                        else {
                            // see setfield below
                            hsmOnEventUD( (hsm_callback_guard_ud) HulaUserIsEvent, (void*) keyname.string );
                            if (is_target_name) {
                                const char * targetname= lua_tostring( L, value_idx );
                                hsmGotoId( hsmState( targetname ) );
                            }
                            else {
                                hsmRunUD( (hsm_callback_action_ud) HulaRun, L );
                            }
                            // store: state_table[ 'eventname' ]= target, regardless if its a 'name' or a function()
                            // because: when we get into HulaUserIsEvent, we need a valid 'eventname'.
                            // for lua 5, unofficially: sharing the string memory in the registry works.
                            // the alternative is: we'd have to copy the string memory out somewhere, and then remember to clean it up.
                            // we might want a named events feature in builder, but avoiding the api complication for now.
                            lua_setfield( L, state_table, keyname.string );  // value is popped.
                        }
                    }                            
                }                
            }
            lua_remove( L, state_table );
        }
    }
    return err;
}


//---------------------------------------------------------------------------
/**
 * chart = { statename = { <statebody> } }
 */
hula_error HulaBuildState( lua_State*L, int chartidx, int * pid ) 
{
    hula_error err= HULA_ERR_UNKNOWN;
    const int check= lua_gettop(L);
    
    //  we want to get the one and only entry, but we dont know its index
    lua_pushnil(L);
    if (lua_next(L, chartidx)) {           // pops top, pull key,value on to the stack
        const int bodyidx = lua_gettop(L);
        const int nameidx = bodyidx-1;
        lua_pushvalue( L, nameidx );
        if (lua_next(L, chartidx )) {
            err= "only state={body} pair can be in the top state";
            lua_pop(L,4); // pop both sets of iterators
        }
        else {
            if (!lua_isstring( L, nameidx )) {
                err= "all keys must be strings";
            }
            else
            if (!lua_istable( L, bodyidx )) {
                err= "expected a state object";
            }
            else {
                nstring_t name;
                lua_tonstring( L, nameidx, &name );
                err= HulaBuildNamedState( L, bodyidx, name.string, name.len, pid );
                lua_pop(L,2); // pop iterators
            }                
        }            
    }        

    assert( check == lua_gettop(L) );
    return err;
}

//---------------------------------------------------------------------------
/**
 * chart = { -init='s1', s1={ <statebody> }, s2={ <statebody> }, ... }
 */
hula_error HulaBuildNamedState( lua_State*L, int idx, const char * name, int namelen, int * pid )
{
    hula_error err= HULA_ERR_UNKNOWN;
    int id= hsmBegin( name, namelen );
    if (id){
        nstring_t nstring= { name, namelen };
        const int check= lua_gettop(L);
        err= _HulaBuildState( L, idx, nstring );
        assert( check == lua_gettop(L) );
        if (!err) {
            if (pid) *pid=id;
        }
        hsmEnd();
    }
    return err;
}
