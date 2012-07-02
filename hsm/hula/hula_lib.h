/**
 * @file hula_lib.h
 *
 * Support for hula.h
 * Implemenets the lua wrappers for hsm statechart classes
 *
 * \internal
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#pragma once
#ifndef __HSM_LUA_LIB_H__
#define __HSM_LUA_LIB_H__

/**
 * default name of the type within lua
 */
#define HULA_LIB        "hsm_statechart"

/**
 * default name of the HULA_LIB's metatable
 */
#define HULA_METATABLE  "hsm.hula"

/**
 * name inside the metatable of the user event matching function
 */
#define HULA_EVENT_TEST "is_user_event"   

#endif // #ifndef __HSM_LUA_LIB_H__
