/**
 * @file hula.h
 *
 * Statemachine builder extensions for lua.
 *
 * \internal
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#pragma once
#ifndef __HSM_LUA_H__
#define __HSM_LUA_H__

typedef struct lua_State lua_State;
typedef const char *  hula_error;

#ifdef WIN32
#pragma comment(lib, "lua51.lib")
#endif

/**
 * The *user* must define when using hula.
 * All events handled by lua code must be identified by string name
 * This function enables the user to determine how an event is mapped to its name.
 * 
 * @param eventname Event name defined in lua.
 * @param status State of statemachine
 * @return HSM_TRUE if the hsm_status_rec::evt matches the passed eventname
 */
hsm_bool HulaUserIsEvent( hsm_status status, const char * eventname );


/**
 * Create an hsm-statechart state from a lua state description.
 * ( Uses hsm-builder to accomplish the task )
 *
 * top_state_example= { top_state_example_name= {...} }
 *
 * @param L Lua state
 * @param idx Index on the stack of the table containing the state description
 * @param pId When return code is 0, filled with the built state(tree) id
 * @return error code
 *
 * @see hsmBegin, HulaBuildNamedState
 */
hula_error HulaBuildState( lua_State*L, int idx, int *pId );

/**
 * Create an hsm-statechart state from a lua state description.
 * ( Uses hsm-builder to accomplish the task )
 *
 * no_top_level_example = { init = 's1', s1 = {...}, s2= {...} }
 *
 * @param L Lua state
 * @param idx Index on the stack of the table containing the state description
 * @param name Name of the state.
 * @param namelen if non-zero, the string is copied for safekeeping
 * @param pId When return code is 0, filled with the built state(tree) id
 * @return error code
 *
 * @see hsmBegin, HulaBuildState
 */
hula_error HulaBuildNamedState( lua_State*L, int idx, const char * name, int namelen, int *pId );

#endif // #ifndef __HSM_LUA_H__
