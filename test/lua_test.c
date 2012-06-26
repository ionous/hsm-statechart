#include "test.h"
#include "samek_plus.h"
#include <hsm\builder\hsm_builder.h>
#include <hsm\hula\hula.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

//---------------------------------------------------------------------------
/**
 * an example user callback
 * @return true if hsm_status::event matches the passed eventname
 */
hsm_bool HulaUserIsEvent( hsm_status status, const char * eventname )
{
    const char ch= status->evt->ch;
    return ch == *eventname;
}

//---------------------------------------------------------------------------
hsm_bool LuaTest()
{
    hsm_bool res= HSM_FALSE;
    
    lua_State *L= lua_open();
    luaL_openlibs(L);
    if (L) { // darn you missing c-99 features
        int x= luaL_loadfile(L, "samek_plus.lua");
        int err= lua_pcall(L, 0, 1, 0);                 // call the file, we expect one return
        if (err) {
            printf("error in lua script");
        }
        else {
            int stateid;
            hsmStartup();
            if (HulaBuildState( L, lua_gettop(L), &stateid )==0) {
                hsm_context_machine_t machine;
                res= TestEventSequence( 
                        HsmMachineWithContext( &machine, 0 ), 
                        hsmResolveId( stateid ), 
                        SamekPlusSequence() );
            }            
            hsmShutdown();
        }
    }        
    lua_close(L);
    return res;
}
