#ifndef _ANOD_COMPILED_FONT_
#define _ANOD_COMPILED_FONT_

#include <stdint.h>

extern uint8_t *canvas;

#ifdef _SSD_1306_128X64_
#define BYTE_LINE ( 128/sizeof(uint8_t) )
#define LIT_POINT(x, y, islit) { \
  if( 0<=(x) && (x)<128 && 0<=(y) && (y)<64 ){  \
    if(islit)                                   \
      canvas[ (63-(y))*BYTE_LINE + (x)/8 ] |=   \
        1 << (7-(x)%8);                         \
    else                                        \
      canvas[ (63-(y))*BYTE_LINE + (x)/8 ] &=   \
        ~( 1 << (7-(x)%8) );                    \
  }                                             \
}
#endif//_SSD_1306_128X64_

#endif//_ANOD_COMPILED_FONT_

