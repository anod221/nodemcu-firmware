#include "module.h"
#include "c_stdlib.h"
#include "platform.h"
#include "lauxlib.h"
#include "rfid.h"

// rfid.setup(pin_ss, pin_rst, mode)
static int ICACHE_FLASH_ATTR rfid_setup( lua_State *L )
{
  int pss = luaL_checkinteger(L, 1);
  int prst = luaL_checkinteger(L, 2);
  SpiMode mode = HALFDUPLEX;
  if( lua_type(L, 3) == LUA_TNUMBER ){
    mode = (SpiMode)luaL_checkinteger(L, 3);
  }
  rfid id = rfid_init(1, mode, pss, prst);

  if( id >= 0 ){
    lua_pushinteger(L, id);
    return 1;
  }
  else {
    const char *str = id == RFID_INIT_NOMOREDEV ? "no empty dev" : "no more mem";
    lua_pushstring(L, str);
    lua_error(L);
    return 0;
  }
}

// rfid.request(rfid, mode)
static int ICACHE_FLASH_ATTR rfid_request_tag( lua_State *L )
{
  rfid id = luaL_checkinteger(L, 1);
  RfidReqMode mode = REQ_CARD_IDLE;
  if( lua_type(L, 2) == LUA_TNUMBER )
    mode = luaL_checkinteger(L, 2);

  luaL_argcheck(L, mode == REQ_CARD_IDLE || mode == REQ_CARD_ALL, 2, "invalid mode");
  res_t r = rfid_request(id, mode);
  if( r != RES_SUCCESS ){
    lua_pushstring(L, "request error");
    lua_error(L);
    return 0;
  }
  else {
    lua_pushinteger(L, r);
    return 1;
  }
}

static const LUA_REG_TYPE rfid_map[] = {
  // for module function
  { LSTRKEY("setup"), LFUNCVAL(rfid_setup) },
  { LSTRKEY("request"), LFUNCVAL(rfid_request_tag) },
  //  { LSTRKEY("anticoll"), },
  //  { LSTRKEY("select"), },
  //  { LSTRKEY("auth"), },
  //  { LSTRKEY("read"), },
  // for module constant
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE( RFID, "rfid", rfid_map, NULL );
