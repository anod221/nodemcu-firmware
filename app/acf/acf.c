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
  char *      filename;
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
#define ACFONT_INDEX_VALUE( f, n ) ( 0x10000*readUInt8( f, ACFONT_INDEX(n)+2 ) + readUInt16( f, ACFONT_INDEX(n)+3 ) )

static inline uint8_t readUInt8( int fd, int pos )
{
  if( pos >= gblfont.filesize ) {
    longjmp( gblfont.except, ACFONT_READ_EOF );
  }
  vfs_lseek( fd, pos, VFS_SEEK_SET );
  return (uint8_t)vfs_getc( fd );
}
static inline int8_t readInt8( int fd, int pos )
{
  if( pos >= gblfont.filesize ) {
    longjmp( gblfont.except, ACFONT_READ_EOF );
  }
  vfs_lseek( fd, pos, VFS_SEEK_SET );
  return (int8_t)vfs_getc( fd );
 }
static inline uint16_t readUInt16( int fd, int pos )
{
  if( pos >= gblfont.filesize ) {
    longjmp( gblfont.except, ACFONT_READ_EOF );
  }
  vfs_lseek( fd, pos, VFS_SEEK_SET );
  int lb = vfs_getc( fd );
  int hb = vfs_getc( fd );
  return (uint16_t)( lb | (hb<<8) );
}
static inline int16_t readInt16( int fd, int pos )
{
  if( pos >= gblfont.filesize ) {
    longjmp( gblfont.except, ACFONT_READ_EOF );
  }
  vfs_lseek( fd, pos, VFS_SEEK_SET );
  int lb = vfs_getc( fd );
  int hb = vfs_getc( fd );
  return (int16_t)( lb | (hb<<8) );
}

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

  gblfont.amount = ACFONT_HEAD_AMOUNT( head );
  gblfont.width = ACFONT_HEAD_WIDTH( head );
  gblfont.height = ACFONT_HEAD_HEIGHT( head );
  gblfont.offsetx = ACFONT_HEAD_OFFSETX( head );
  gblfont.offsety = ACFONT_HEAD_OFFSETY( head );

  vfs_lseek( font, 0, VFS_SEEK_END );
  gblfont.filesize = vfs_tell( font );

  if( gblfont.filename != NULL )
    c_free( gblfont.filename );

  int len = strlen( acfile );
  char *file = (char*)c_malloc( len+1 );
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
static inline const char* next_unicode( const char *utf8, uint32_t *code )
{
  const uint8_t *str = utf8;
  
  uint8_t c = *utf8++;
  if( c == 0 ){
    return NULL;
  }
  else if( c < 0x80u ) {
    *code = c;
  }
  else {
    int n = 0;
    *code = 0;
    for(uint8_t t = c & 0x80; t == 0x80; t = c & 0x80)
      {
        c <<= 1;
        ++n;
      }
    for(c >>= n; n-->0; c = (*utf8++) & 0x3f )
      {
        *code <<= 6;
        *code |= c;
      }
    --utf8;
  }
  return utf8;
}

static inline int bsearch_font( int fd, uint32_t unicode )
{
  int min = 0, max = gblfont.amount;
  for( int m=(min+max)>>1; min < m; m = (min+max)>>1 ){
    uint32_t code = readUInt16( fd, ACFONT_INDEX(m) );
    if( unicode < code ) max = m;
    else if( code < unicode ) min = m;
    else return ACFONT_INDEX_VALUE( fd, m );
  }
  return -1;
}

static int render_unicode( int fd, int *x, int *y, unsigned width, unsigned height, uint32_t code, unsigned *width_max )
{
  int result = setjmp( gblfont.except );
  if( result == 0 ){
    // 获取code对应的字体信息
    int font_pos = bsearch_font( fd, code );
    if( font_pos < 0 ) return 0;
    if( font_pos > gblfont.filesize ) return 0;

    int font_base = ACFONT_HEAD_SIZE + gblfont.amount * ACFONT_SIZEOF_INDEX;
    font_base += font_pos;
    int8_t bbx[4];
    vfs_lseek( fd, font_base, VFS_SEEK_SET );
    vfs_read( fd, bbx, sizeof( bbx ) );

    int sl = vfs_getc( fd );
    int size = (sl%2) + 1;
    int len = sl >> 1;

    // render
    // 从上往下(y从大到小，x从小到大)进行绘制
    int px = *x, py = *y;
    int cx = px + bbx[2], cy = py + bbx[1] + bbx[3];

    // 从cx/cy开始绘制
    // 先检查宽度
    if( width_max != NULL ){
      if( cx + bbx[0] >= *width_max )
        return 1;
    }

    // 绘制
    uint32_t mask = 1 << (8*( (bbx[0]-1)>>3 )+7);
    for( int i=0; i < bbx[1]; ++i ){
      int data = size == 1 ? vfs_getc(fd) : (vfs_getc(fd) | vfs_getc(fd)<<8);
      for( int j=0; j < bbx[0]; ++j ){
        ACF_LIT_POINT( cx+j, cy-i, width, height, (data & mask) == mask );
        data <<= 1;
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
