/* 
 * ACF就是把bdf字体进行二进制压缩。其文件分为三个部分
 * 1文件头，总共6字节，前2字节是字体数量，后面4字节是bdf的FONTBOUNDINGBOX
 * 2数据索引，总共是字体数量*5个字节。其中5个字节内容是前2字节的unicode编码和后3字节的数据偏移量
 * 3数据，每个数据是前4个字节的bbx，第5个字节的前7位是DWIDTH的x分量，后1位0表示位图每次读取1字节，为1表示每次读取2字节。第6个字节及以后是BITMAP的数据，总长度是bbx[1]的长度。
 */
#include "c_stdint.h"
#include "c_stdio.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "vfs.h"

#include <setjmp.h>

#include "acf.h"

// 画点函数
#ifndef ACF_LIT_POINT
#error "ACF_LIT_POINT SHOULD BE DECLARE BEFORE USING GB2312 FONT"
#endif

typedef struct {
  uint8_t     width;
  uint8_t     height;
  int8_t      offsetx;
  int8_t      offsety;
  uint16_t    amount;
  char       *filename;
  uint16_t   *chapter;
  size_t      filesize;
  jmp_buf     except;
} ACFont;

static ACFont gblfont = {
  0,0,0,0,0,NULL,0
};

#define ACFONT_HEAD_SIZE 6
#define ACFONT_HEAD_AMOUNT( buf )  ( (buf)[0]+(buf)[1]*0x100 )
#define ACFONT_HEAD_WIDTH( buf )   ( (buf)[2] )
#define ACFONT_HEAD_HEIGHT( buf )  ( (buf)[3] )
#define ACFONT_HEAD_OFFSETX( buf ) ( (int8_t)(buf)[4] )
#define ACFONT_HEAD_OFFSETY( buf ) ( (int8_t)(buf)[5] )

#define ACFONT_SIZEOF_INDEX 5
#define ACFONT_INDEX( n )          ( ACFONT_HEAD_SIZE + (n)*ACFONT_SIZEOF_INDEX )
#define ACFONT_INDEX_VALUE( data ) ( 0x10000*data[2] + data[3] + data[4]*0x100 )

#define ACFONT_SIZEOF_BBX 4
#define ACFONT_GLYPH_SIZE 40

#define ACFONT_CHAPTER_MEMORY 320

// 提供两个方法：
// 1 设置字体文件名称
// 2 使用该字体在一个原点范围内渲染一行文字

// 设置字体名称
int acf_set_font( const char *acfile )
{
  // open the font file
  int font = vfs_open( acfile, "rb" );
  if( font == 0 ) return ACFONT_NOT_FOUND;
 
  // 读取数据
  uint8_t head[ACFONT_HEAD_SIZE];
  if( vfs_read( font, head, ACFONT_HEAD_SIZE ) != ACFONT_HEAD_SIZE ){
    vfs_close( font );
    return ACFONT_INVALID;
  }
  if( ACFONT_HEAD_HEIGHT( head )*2 > ACFONT_GLYPH_SIZE ){
    vfs_close( font );
    return ACFONT_NOT_SUPPORT;
  }

  gblfont.amount = ACFONT_HEAD_AMOUNT( head );
  gblfont.width = ACFONT_HEAD_WIDTH( head );
  gblfont.height = ACFONT_HEAD_HEIGHT( head );
  gblfont.offsetx = ACFONT_HEAD_OFFSETX( head );
  gblfont.offsety = ACFONT_HEAD_OFFSETY( head );

  vfs_lseek( font, 0, VFS_SEEK_END );
  gblfont.filesize = vfs_tell( font );

  if( gblfont.chapter != NULL )
    c_free( gblfont.chapter );
  if( gblfont.filename != NULL )
    c_free( gblfont.filename );

  // get index chapters
  // 1 decide chapter count
  const int nparts = ACFONT_CHAPTER_MEMORY / ACFONT_SIZEOF_INDEX;
  int nchapter = gblfont.amount / nparts;
  if( nchapter * nparts < gblfont.amount ){
    ++nchapter;
  }

  // 2 alloc cache for chapter
  uint16_t *pchapter = (uint16_t*)c_malloc( nchapter * sizeof(uint16_t) );
  if( pchapter == NULL ){
    memset( &gblfont, 0, sizeof(gblfont) );
    vfs_close( font );
    return ACFONT_MEM_EMPTY;
  }

  // 3 load chapter to cache
  for( int i=0; i < nchapter; ++i ){
    int pos = ACFONT_INDEX(i * nparts);
    vfs_lseek( font, pos, VFS_SEEK_SET );
    vfs_read( font, head, 2 );
    pchapter[i] = head[0] | (head[1]<<8);
  }
  gblfont.chapter = pchapter;

  // save filename for `fopen`
  int len = strlen( acfile );
  char *file = (char*)c_malloc( len+1 );
  if( file == NULL ){
    c_free( gblfont.chapter );
    memset( &gblfont, 0, sizeof(gblfont) );
    vfs_close( font );
    return ACFONT_MEM_EMPTY;
  }

  memcpy( file, acfile, len );
  file[len] = 0;
  gblfont.filename = file;
  
  vfs_close( font );
  return 0;
}

/*
   |  Unicode符号范围      |  UTF-8编码方式  
 n |  (十六进制)           | (二进制)  
---+-----------------------+------------------------------------------------------  
 1 | 0000 0000 - 0000 007F |                                              0xxxxxxx  
 2 | 0000 0080 - 0000 07FF |                                     110xxxxx 10xxxxxx  
 3 | 0000 0800 - 0000 FFFF |                            1110xxxx 10xxxxxx 10xxxxxx  
 4 | 0001 0000 - 0010 FFFF |                   11110xxx 10xxxxxx 10xxxxxx 10xxxxxx  
 5 | 0020 0000 - 03FF FFFF |          111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx  
 6 | 0400 0000 - 7FFF FFFF | 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 */
#define BYTE_H 1u<<7
#define TEST_H(c) ((c) & BYTE_H)
#define ISSET_H(c) TEST_H(c) == BYTE_H
#define EXBYTE 0x3fu
#define EXDATA(c) ((c) & EXBYTE)
static inline const char* next_unicode( const char *utf8, uint32_t *code )
{
  if( utf8 == NULL || *utf8 == '\0' )
    return NULL;

  uint8_t c = *utf8++;

  if( c > 0x7f ) {
    int n;
    for(n = 0; ISSET_H(c); c <<= 1 )
      ++n;
    if( n > 1 ){
      for( *code = 0, c >>= n;
           n-->0;
           c = EXDATA(*utf8++) )
        *code = (*code<<6) | c;
      --utf8;
    }
    else *code = c;
  }
  else *code = c;
  
  return utf8;
}

static int bsearch_font( int fd, uint32_t unicode )
{
  // decide chapter count
  const int nparts = ACFONT_CHAPTER_MEMORY / ACFONT_SIZEOF_INDEX;
  int nchapter = gblfont.amount / nparts;
  if( nchapter * nparts < gblfont.amount ) {
    ++nchapter;
  }

  int min = 0, max = nchapter;
  int m, found = 0;

    // search in chapter
  for( m=(min+max)>>1; min < m; m=(min+max)>>1 ){
    uint16_t code = gblfont.chapter[m];
    if( unicode < code ) max = m;
    else if( code < unicode ) min = m;
    else { found = 1; break; }
  }
  if( found ) {
    uint8_t dat[ ACFONT_SIZEOF_INDEX ];
    vfs_lseek( fd, ACFONT_INDEX(nparts*m), VFS_SEEK_SET );
    vfs_read( fd, dat, ACFONT_SIZEOF_INDEX );
    return ACFONT_INDEX_VALUE( dat );
  }

  // not match in chapter, search in chapter's parts
  int len = m+1 == nchapter ? (gblfont.amount % nparts) : nparts;
  int size = len * ACFONT_SIZEOF_INDEX;
  uint8_t *cache = (uint8_t*)c_malloc( size );
  vfs_lseek( fd, ACFONT_INDEX(nparts*m), VFS_SEEK_SET );
  vfs_read( fd, cache, size );

  for( min=0, max=len, m=len>>1;
       min < m;
       m = (min+max) >> 1 )
    {
      int idx = m * ACFONT_SIZEOF_INDEX;
      uint16_t code = cache[idx] | (cache[idx+1] << 8);
      if( unicode < code ) max = m;
      else if( code < unicode ) min = m;
      else {
        uint8_t *index = &cache[idx];
        found = ACFONT_INDEX_VALUE( index );
        break;
      }
    }
  
  c_free( cache );
  return found ? found : -1;
}

static inline int render_unicode( int fd, int *x, int *y, unsigned width, unsigned height, uint32_t code, unsigned *width_max )
{
  int result = setjmp( gblfont.except );
  if( result == 0 ){
    // 获取code对应的字体信息
    int font_pos = bsearch_font( fd, code );
    if( font_pos < 0 ) return 0;
    if( font_pos > gblfont.filesize ) return 0;
    font_pos += ACFONT_HEAD_SIZE + gblfont.amount * ACFONT_SIZEOF_INDEX;

    int8_t bbx[ ACFONT_SIZEOF_BBX ];
    vfs_lseek( fd, font_pos, VFS_SEEK_SET );
    vfs_read( fd, bbx, ACFONT_SIZEOF_BBX );

    int sl = vfs_getc( fd );
    int size = (sl%2) + 1;
    int len = sl >> 1;

    // render
    // 从上往下(y从大到小，x从小到大)进行绘制
    int px = *x, py = *y;                            // px/py不参与计算位置
    int cx = px + bbx[2], cy = py + bbx[1] + bbx[3]; // cx/cy计算位置进行绘制

    // 先检查宽度
    if( ( width_max != NULL ) && ( cx + bbx[0] >= *width_max ) )
      return 1;

    // 获取字体位图数据
    uint8_t glyph[ ACFONT_GLYPH_SIZE ];
    uint8_t szbmp = bbx[1] * size;
    vfs_read( fd, glyph, szbmp );

    // 绘制
    uint32_t mask = 1 << (8*( (bbx[0]-1)>>3 )+7);
    if( size == 1 ){
      for( int i=0; i < bbx[1]; ++i ){
        for( int j=0; j < bbx[0]; ++j ){
          ACF_LIT_POINT( cx+j, cy-i, width, height, (glyph[i] & mask) == mask );
          glyph[i] <<= 1;
        }
      }
    }
    else {
      for( int i=0; i < bbx[1]; ++i ){
        int data = glyph[i<<1] | ( glyph[(i<<1)|1]<<8 );
        for( int j=0; j < bbx[0]; ++j ){
          ACF_LIT_POINT( cx+j, cy-i, width, height, (data & mask) == mask );
          data <<= 1;
        }
      }
    }

    // 更新
    *x = px + len;
    *y = py;
    return 0;
  }
  else if( result == ACFONT_READ_EOF ){
    return 0;
  }
}

// 根据字体绘制中文字符
// x,y - 绘制一行字符的基准点
// width - 最长绘制多少个像素点，填0则忽略此参数
// utf8_line - utf8字符串
// 返回：第一个未绘制的字符的位置，如果width为0，则返回永远是NULL
const char* acf_draw( int x, int y, unsigned width, unsigned height, unsigned maxwidth, const char *utf8_line )
{
  int font = vfs_open( gblfont.filename, "rb" );
  if( font == 0 ) {
    c_printf("open file failed %s", gblfont.filename );
    return utf8_line;
  }
  
  uint32_t unicode;
  unsigned *option = width == 0 ? NULL : &width;
  for( const char *next = next_unicode(utf8_line, &unicode);
       next != NULL;
       next = next_unicode(utf8_line, &unicode) )
    {
      int error = render_unicode( font, &x, &y, width, height, unicode, option );
      if( error ) {
	vfs_close( font );
	return utf8_line;
      }

      utf8_line = next;
    }
  vfs_close( font );
  return NULL;
}
