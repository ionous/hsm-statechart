/**
 * @file lua_test.c
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 *
 * The main test in this file has a statemachine specified in lua
 * unlike the sample code, however, the statemachine *runs* in c.
 * To handle the translation between the test uses two statemachines.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */

#include "test.h"
#include "samek_plus.h"
#include <hsm\hsm_machine.h>
#include <hsm\builder\hsm_builder.h>
#include <hsm\hula\hula.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

//---------------------------------------------------------------------------
/**
 * Context data used for the C-statemachine
 */
typedef struct lua_context_rec lua_context_t;
struct lua_context_rec {
  hsm_context_t core;
  lua_State * L;
  hsm_machine lua;
  hsm_state first;
};  

//---------------------------------------------------------------------------
/**
 * The C-statemachine definition
 * It exists to bounce data to a lua based statemachine
 * This is a nice general pattern, if slightly verbose,
 * it provides the ability for one c-event to become many lua events
 * or, many c-events to filter down into one lua event.
 */
HSM_STATE_ENTER( LuaBounce, HsmTopState, 0 );

hsm_context LuaBounceEnter( hsm_status status )
{
  lua_context_t*ctx= (lua_context_t*) status->ctx;

  #if 0 // optional for first enter: create an event table
    lua_createtable( ctx->L,1,0 );
    lua_pushstring( ctx->L,"init" );
    lua_rawseti( ctx->L, -2, 1 ); 
  #endif  

  HsmStart( ctx->lua, ctx->first );
  return status->ctx;
}

hsm_state LuaBounceEvent(hsm_status status)
{
  hsm_state next= 0;
  lua_context_t*ctx= (lua_context_t*) status->ctx;

  // translate the event into a hula event table
  // the first field is the event name
  // event payload data can follow 
  // (ex. mouse buttons for a mouse event )
  lua_createtable( ctx->L, 1, 0 );
  lua_pushlstring( ctx->L, &status->evt->ch, 1 );
  lua_rawseti( ctx->L, -2, 1 ); 
  
  // send the event to the lua machine
  if (HsmSignalEvent( ctx->lua, status->evt )) {
    next= HsmStateHandled();
  }
  else if( !HsmIsRunning(ctx->lua)) {
    next= HsmStateError();
  }
  
  // pop the event table
  lua_pop( ctx->L, 1 );

  // return our next state; still 0 if unhandled
  return next;
}

//---------------------------------------------------------------------------
hsm_bool LuaTest()
{
  hsm_bool res= HSM_FALSE;
  
  lua_State *L= lua_open();
  luaL_openlibs(L);
  if (L) { // darn you missing c-99 features
    int x= luaL_loadfile(L, "samek_plus.lua");
    int err= lua_pcall(L, 0, 1, 0);         // call the file, we expect one return
    if (err) {
      printf("error in lua script: %d", err);
    }
    else {
      if (hsmStartup()) {
        int stateid;
        hula_error err= HulaBuildState( L, lua_gettop(L), &stateid );
        if (!err) {
          hsm_context_machine_t lua;
          if (HsmMachineWithContext( &lua, 0 )) {
            hsm_context_machine_t bounce;
            hsm_state first= hsmResolveId(stateid);
            lua_context_t ctx= { 0,0, L, &lua.core, first };
            if (HsmMachineWithContext( &bounce, &ctx.core )) {
              bounce.core.flags|= TEST_HSM_NO_LOGGING;
              res= TestEventSequence( &bounce.core, LuaBounce(), SamekPlusSequence() );
            }                
          }            
        }      
        hsmShutdown();
      }
    }
  }    
  lua_close(L);
  return res;
}
