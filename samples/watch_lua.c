/**
 * @file watch1_named_events.c
 *
 * Copyright (c) 2012, everMany, LLC.
 * All rights reserved.
 * 
 * Code licensed under the "New BSD" (BSD 3-Clause) License
 * See License.txt for complete information.
 */
#include <hsm/hsm_machine.h> // the state machine
#include <hsm\builder\hsm_builder.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <hsm\hula\hula.h>

#include "platform.h"       // console input/output

//---------------------------------------------------------------------------
int lua_platform_get_key(lua_State *L)
{
    int k= PlatformGetKey();
    lua_pushinteger( L, k );
    return 1;
}

//---------------------------------------------------------------------------
int lua_platform_sleep( lua_State *L ) 
{
    int s= luaL_checkinteger( L,1 );
    PlatformSleep(s);
    return 0;
}    

//---------------------------------------------------------------------------
int watch_lua( int argc, char* argv[] )
{   
    int res=-1;
    static luaL_Reg lua_platform[]= {
        { "get_key", lua_platform_get_key },
        { "sleep", lua_platform_sleep },
        { 0 }
    };
    
    lua_State *L= lua_open();
    luaL_openlibs(L);
    luaL_register( L, "platform", lua_platform );
    HulaRegister( L, 0 );    
    if (hsmStartup()) {
        int x= luaL_loadfile(L, "hula//watch.lua");
        res= lua_pcall(L, 0, 1, 0);                 // call the file, we expect one return
        if (res) {
            const char * msg= lua_tostring( L, -1 );
            printf("error in lua script %s", msg ? msg :"unknown");
        }
        hsmShutdown();
    }            
    lua_close(L);
    return res;
}
