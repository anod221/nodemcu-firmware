#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include <c_stdio.h>
#include <c_stdlib.h>
#include <ctype.h>
#include <stdint.h>

#include <acf.h>

uint8_t *acfCanvas = NULL;
uint16_t acfCanvasW = 0;
uint16_t acfCanvasH = 0;

// acfcanvas.setup( "wqy11.acf", width, height )
static int ICACHE_FLASH_ATTR acfc_setup( lua_State *L )
{
  luaL_checkstring(L, 1);
  const char *p = lua_tostring(L, 1);
  
  int width = luaL_checkint(L, 2);
  int height = luaL_checkint(L, 3);
  if( (width % 8) != 0 ){
    lua_pushstring( L, "width shoud times 8" );
    lua_error(L);
    return 0;
  }
  if( width < 0 || height < 0 ){
    lua_pushstring( L, "width/height should greater than 0");
    lua_error(L);
    return 0;
  }
  acfCanvasW = width;
  acfCanvasH = height;
  
  int result = acf_set_font( p );
  if( result == 0 ){
    acfCanvas = c_malloc( ACF_CANVAS_SIZE(acfCanvasW, acfCanvasH) );
    if( acfCanvas == NULL ) {
      lua_pushstring(L, "mem not enough");
      lua_error(L);
      return 0;
    }

    memset( acfCanvas, 0, ACF_CANVAS_SIZE(acfCanvasW, acfCanvasH) );
    lua_pushinteger( L, 0 );
    return 1;
  }
  else {
    switch( result ){
    case ACFONT_NOT_FOUND:
      lua_pushstring( L, "font not exist" ); break;
    case ACFONT_INVALID:
    case ACFONT_READ_EOF:
    default:
      lua_pushstring( L, "invalid font" ); break;
    }
    lua_error( L );
    return 0;
  }
}

// acfcanvas.draw(x, y, width, str)
static int ICACHE_FLASH_ATTR acfc_draw( lua_State *L )
{
  if( acfCanvas == NULL ){
    lua_pushstring( L, "call acfcanvas.setup first");
    lua_error(L);
    return 0;
  }
  int x = luaL_checkint( L, 1 );
  int y = luaL_checkint( L, 2 );
  int width = luaL_checkint( L, 3 );
  luaL_checkstring(L, 4);
  const char *str = lua_tostring(L, 4);

  const char *rest = acf_draw( x, y, acfCanvasW, acfCanvasH, width, str );

  if( rest ){
    lua_pushstring( L, rest );
    return 1;
  }
  else return 0;
}

// local data = acfcanvas.u8gbmp()
// u8g.disp.drawBitmap(0, 0, 16, 64, data)
static int ICACHE_FLASH_ATTR acfc_bitmap( lua_State *L )
{
  if( acfCanvas == NULL ){
    lua_pushstring( L, "call acfcanvas.setup first");
    lua_error(L);
    return 0;
  }
  lua_pushlstring(L, acfCanvas, ACF_CANVAS_SIZE(acfCanvasW, acfCanvasH));
  return 1;
}

// acfcanvas.clear( [rowStart, rowEnd] )
static int ICACHE_FLASH_ATTR acfc_clear( lua_State *L )
{
  if( acfCanvas == NULL ){
    lua_pushstring( L, "call acfcanvas.setup first");
    lua_error(L);
    return 0;
  }
  int rowStart = 0;
  int rowEnd = acfCanvasH;
  int numrow = rowEnd - rowStart;
  if( lua_gettop(L) == 1 ){
    rowStart = luaL_checkint(L, 1);
    if( rowStart >= acfCanvasH || rowStart < 0 ){
      lua_pushstring( L, "out of range" );
      lua_error(L);
      return 0;
    }
    rowStart = 63 - rowStart;
    numrow = 1;
  }
  else if( lua_gettop(L) > 1 ){
    rowStart = luaL_checkint(L, 1);
    rowEnd = luaL_checkint(L, 2);
    if( rowEnd <= rowStart ){
      lua_pushstring(L, "argument 2 should greater than argument 1");
      lua_error(L);
      return 0;
    }
    if( rowStart < 0 || rowStart > acfCanvasH
	|| rowEnd < 0 || rowEnd >= acfCanvasH ){
      lua_pushstring( L, "out of range" );
      lua_error(L);
      return 0;
    }
    numrow = rowEnd - rowStart;
    rowEnd--;
    rowStart = 63 - rowEnd;
  }

  int szrow = ACF_BYTE_WIDTH( acfCanvasW, acfCanvasH );
  memset( acfCanvas + rowStart*szrow, 0, szrow*numrow );
  return 0;
}

static const LUA_REG_TYPE acf_canvas_map[] = {
  { LSTRKEY("setup"), LFUNCVAL(acfc_setup) },
  { LSTRKEY("draw"), LFUNCVAL(acfc_draw) },
  { LSTRKEY("u8gbmp"), LFUNCVAL(acfc_bitmap) },
  { LSTRKEY("clear"), LFUNCVAL(acfc_clear) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE( ACFCANVAS, "acfcanvas", acf_canvas_map, NULL );
