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

#include <assert.h>
#include <string.h>
#include <malloc.h>

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
static int HulaGetStateTable( lua_State * L, hsm_state state )
{
    const int tables= HulaGetStateTables( L ); // pull tables onto the stack
    lua_getfield( L, tables, state->name );    // get tables[ state->name ]
    lua_remove( L, tables );                   // remove the tables
    return lua_gettop(L);                      // return statetable
}

/**
 * @internal
 * create a new state table
 * the table is stored in the registry: registry[ tables [ statename ]  ]
 */
static int HulaCreateStateTable( lua_State * L, nstring_t statename )
{
    int res;
    const int check= lua_gettop(L);
    const int tables= HulaGetStateTables( L );  // pull tables onto the stack
    lua_pushnstring( L, statename );            // key=statename
    lua_newtable(L);                            // value={}
    lua_settable( L, tables );                  // tables[key]=value
    lua_getfield( L, tables, statename.string );// pull the value{} back
    lua_remove( L, tables );                    // remove the tables
    res= lua_gettop(L);                         // what remains is our value{}
    assert( res == check+1 );
    return res;
}

/**
 * @internal
 * pull a state table function to call on the stack
 */
int HulaGetCall( lua_State* L, hsm_state state, int rawi, const char * name )
{
    const int state_table= HulaGetStateTable( L, state );
    if (!name) {
        lua_rawgeti( L, state_table, rawi ); 
    }
    else {
        lua_pushstring( L, name );
        lua_gettable( L, state_table );
    }
    lua_remove( L,state_table );
    return lua_gettop( L );
}

//---------------------------------------------------------------------------
// Run time callbacks:
//---------------------------------------------------------------------------

static hsm_context HulaEnterUD( hsm_status status, void * user_data );
static hsm_state HulaRunUD( hsm_status status, void * user_data );
static void HulaExit( hsm_status status );

typedef struct hula_ctx_rec HulaContext;
struct hula_ctx_rec 
{
    /**
     * core context data: hula_ctx_rec extends the base context.
     */
    hsm_context_t core;

    /**
     * lua state that's managed by this state
     * ( this is generally constant across the whole machine )
     */
    lua_State *L;

    /**
     * a luaL_ref to lua data generated by the state's entry
     * currently uses the registry, would it be better (faster?) to use state tables?
     */
    int lua_ref;

    /**
     * count the number of times this hula_ctx_rec is shared amongst parents and children
     * dont release the lua data until the last state using the lua_ref data is done.
     */
    int lua_ref_count;    
};

//---------------------------------------------------------------------------

/**
 * @internal
 * call the lua specified entry function with this state's parent context data
 * store the data returned from that function as this state's context
 *
 * note: every hula event callback needs a lua_State.
 * we can use our parent lua context if it exists; 
 * if it doesnt exist, we'll have to make one.
 */
static hsm_context HulaEnterUD( hsm_status status, void * user_data )
{
    hsm_context ret=0;
    HulaContext* new_ctx=0, *parent_ctx=0;
    lua_State* L= (lua_State*)user_data;
    const int check= lua_gettop(L);

    // get the lua specified enter= function()
    const int lua_entryfn= HulaGetCall( L, status->state, LUA_T_ENTER, 0 );
    
    // get our parent's lua data (if its also a hula state)
    if (status->state->parent && status->state->parent->exit==HulaExit) {
        // when a state gets entered, the context is its parent's context
        parent_ctx= (HulaContext*) status->ctx;
    }

    // if our parent has lua data, then use that as this state's initial data
    // if not: use our parent's state's c-object
    if (parent_ctx) {
        lua_rawgeti( L, LUA_REGISTRYINDEX, parent_ctx->lua_ref );
    }
    else {
        // i wonder what's better? the c context? or nil?
        lua_pushlightuserdata( L, status->ctx );
    }

    // no entry function means no unique data for this state:
    // re-use our parent's context ( if its not null )
    if (!lua_isfunction( L, lua_entryfn )) {
        lua_remove( L, lua_entryfn );
        if (parent_ctx) {
            lua_pop(L,1);
            new_ctx= parent_ctx;
            ++parent_ctx->lua_ref_count;
        }
    }
    else {
        int err= lua_pcall(L, 1,1,0); 
        if (err) {
            const char * msg=lua_tostring(L,-1);
            lua_pop(L,1);//? TODO: and do what on error exactly?
        }    
    }

    // if we have a context pointer, then it came from our parent
    // we didn't have any unique data, so pop that data
    if (!new_ctx) {
        // if we dont have a context, well... we need one.
        // use the data that's on the stack: be it the results of the pcall, or a parent's C data
        new_ctx= (HulaContext*) HsmContextAlloc( sizeof(HulaContext) );
        assert( new_ctx );
        if (!new_ctx) {
            lua_pop(L,1);
        }
        else {
            new_ctx->L= L;            
            new_ctx->lua_ref= luaL_ref( L, LUA_REGISTRYINDEX ); 
            ret= &new_ctx->core;
        }
    }

    assert( check== lua_gettop(L) );            // is life good?

    // the machine will store our context
    return ret;
}

//---------------------------------------------------------------------------
/**
 * @internal
 * callback for every action in lua that was assigned a function()
 * @param status hsm_status_rec::ctx contains HulaContext setup in HulaEnter
 * @param user_data is the const char * of the event string
 */
static hsm_state HulaRunUD( hsm_status status, void * user_data )
{
    hsm_state ret=0;
    const char * event_name= (const char*) user_data;
    if (HulaUserIsEvent( status, event_name )) {
        HulaContext*ctx= (HulaContext*)(status->ctx);
        lua_State* L= ctx->L;
        const int check= lua_gettop(L);
    
        // pull the function to call:
        const int lua_eventfn= HulaGetCall( L, status->state, 0, event_name );
            
        // pull lua data to parameterize the call:
        lua_rawgeti( L, LUA_REGISTRYINDEX, ctx->lua_ref );

        // run the function: 1 arg, 1 result
        if (!lua_isfunction( L, lua_eventfn )) {
            lua_remove( L, lua_eventfn );
        }
        else {
            int err= lua_pcall(L, 1, 1, 0); 
            if (err) {
                const char * msg=lua_tostring(L,-1);
                lua_pop(L,1);//? TODO: and do what on error exactly?
                ret= HsmStateError();
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
                // pop the function results
                lua_pop(L,1);
            }
        }
        assert( check== lua_gettop(L) );            // is life good?
    }
    
    // return the next state
    return ret;
}

//---------------------------------------------------------------------------
/**
 *  @internal
 */
static void HulaExit( hsm_status status )
{
    HulaContext*ctx= (HulaContext*)(status->ctx);
    lua_State* L= ctx->L;
    const int check= lua_gettop(L);

    // pull the function to call:
    const int lua_exitfn= HulaGetCall( L, status->state, LUA_T_EXIT, 0 );

    // call the function: one arg, zero results
    if (!lua_isfunction( L, lua_exitfn )) {
        lua_remove( L, lua_exitfn );
    }
    else {
        int err;
        // pull context data to pass it to function
        lua_pushlightuserdata( L, ctx );
        lua_gettable( L, LUA_REGISTRYINDEX );

        err= lua_pcall(L, 1,0,0); //? TODO: and do what on error exactly?
        if (err) {
            const char * msg=lua_tostring(L,-1);
            lua_pop(L,1);//? TODO: and do what on error exactly?
        }
    }    

    // release the old lua data
    if (--ctx->lua_ref_count <0) {
        luaL_unref( L, LUA_REGISTRYINDEX, ctx->lua_ref );
        ctx->lua_ref= LUA_NOREF;
    }        
    assert( check== lua_gettop(L) );            // is life good?
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

            // force each and every lua function to have the context management it needs
            hsmOnEnterUD( HulaEnterUD, L );
            hsmOnExit( HulaExit );

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
                        }
                        else 
                        // Exit: ex. { enter = function() end }
                        if (is_target_function && NSTRING_IS( keyname, EXIT )) {
                            // state_table['exit']= function
                            lua_rawseti( L, state_table, LUA_T_EXIT ); // value is popped.
                        }
                        // Event: ex. { event = 'name' }, or: { event = function() end }
                        else {
                            const char *event_name= keyname.string;
                            // see setfield below
                            if (!is_target_name) {
                                hsmOnEventUD( HulaRunUD, (void*) event_name );
                            }
                            else {
                                // note: HulaRunUD could probably handle all of this itself....  but why not...
                                const char * targetname= lua_tostring( L, value_idx );
                                hsmIfUD( (hsm_callback_guard_ud) HulaUserIsEvent, (void*) event_name );
                                hsmGotoId( hsmState( targetname ) );
                            }
                            // store: state_table[ 'event_name' ]= target, regardless if its a 'name' or a function()
                            // because: when we get into HulaUserIsEvent, we need a valid 'event_name'.
                            // for lua 5, unofficially: sharing the string memory in the registry works.
                            // the alternative is: we'd have to copy the string memory out somewhere, and then remember to clean it up.
                            // we might want a named events feature in builder, but avoiding the api complication for now.
                            lua_setfield( L, state_table, event_name );  // value is popped.
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
