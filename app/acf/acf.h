#ifndef _ANOD_COMPILED_FONT_H_
#define _ANOD_COMPILED_FONT_H_

#include <stdint.h>

#define ACFONT_NOT_FOUND -1
#define ACFONT_INVALID   -2
#define ACFONT_READ_EOF  -3

extern uint8_t *acfCanvas;
extern int acf_set_font( const char * );
extern const char* acf_draw( int, int, unsigned, unsigned, unsigned, const char* );

#include "acf_dev.h"
#include "acf_cfg.h"

#if ACF_CANVAS == ACFDEV_U8GBITMAP

#define ACF_BYTE_SIZE                  8
#define ACF_CANVAS_SIZE(w, h)          ( (w)*(h)/ACF_BYTE_SIZE )
#define ACF_BYTE_WIDTH(w, h)           ( (w)/ACF_BYTE_SIZE )
#define ACF_BYTE_OFFSET(x, y, w, h)    ( ((h)-(y)-1)*ACF_BYTE_WIDTH(w, h) + (x)/ACF_BYTE_SIZE )
#define ACF_U8G_BMP_BIT(x, y)          ( 1<<(7-(x)%8) )

#define ACF_LIT_POINT( x, y, w, h, islit ) {				\
    if( 0<=(x) && (x)<w && 0<=(y) && (y)<h ){				\
      if( (islit) )							\
	acfCanvas[ ACF_BYTE_OFFSET(x,y,w,h) ] |= ACF_U8G_BMP_BIT(x,y);	\
      else								\
	acfCanvas[ ACF_BYTE_OFFSET(x,y,w,h) ] &= ~ACF_U8G_BMP_BIT(x,y); \
    }									\
  }
#else
#endif

#endif//_ANOD_COMPILED_FONT_H_
