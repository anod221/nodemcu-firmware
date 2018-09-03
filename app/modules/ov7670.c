#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_stdlib.h"
#include "task/task.h"

// ov7670.bind( )
static int ICACHE_FLASH_ATTR ov7670_bind( lua_State *L )
{
  
}

static const LUA_REG_TYPE ov7670_map[] = {
  { LSTRKEY("bind"), LFUNCVAL(ov7670_bind) },
  {LNILKEY, LNILVAL}
};

NODEMCU_MODULE( OV7670, "ov7670", ov7670_map, NULL );
