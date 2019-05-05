# **NodeMCU 2.2.0** extra module #

## irrecv
This module is used for infrared receiving.

`irrecv.bind( pin, protocol, callback )`   
`pin` - The pin connect to the signal line of the receiver.   
`protocol` - The infrared protocol.Now only irrecv.NEC is surported.   
`callback` - The callback function for receiving. The parameters are bytes received.   
`return` - nil

```lua
irrecv.bind( 3, irrecv.NEC, function( cli, clirev, cmd, cmdrev ) print( cli, cmd ) end
```

## irsend
This module is used for infrared sending.    

`irsend.setup()`   
`return` - nil   
Prepare for sending. After this call, user should connect the vcc pin of sender to gpio 2.   

`irsend.send( b32, protocol )`   
`b32` - The data to send.It should be a 32-bit integer.   
`protocol` - The protocol for sending.Now only irsend.NEC is surported.   
`return` - nil   

```lua
irsend.setup()
irsend.send( 0x00ff5aa5, irsend.NEC )
```

## tinynmea
This module is used for parsing GPRMC data.   

```lua
uart.on( "data", 192, function(data)
  local date, time, longitude, latitude, speed, cmg, geomagnetic = tinynmea.parse(data)
  if date then
    print( date, time, longitude, latitude, speed, cmg, geomagnetic )
  end
end, 0)
```

## acfcanvas
This module is a util of font rendering for u8g.

`acfcanvas.setup( fontname_str, canvas_width, canvas_height )`
`fontname_str` - The acf font file on flash.
`canvas_width` - The width of the canvas for rendering.
`canvas_height` - The height of the canvas for rendering.
`return` - 0 for success, otherwise failed.
Create the canvas, using the font with the file 'fontname_str' and the size with canvas_width/canvas_height.

`acfcanvas.clear( [rowstart[, rowend]] )`
`rowstart` - The start row to be cleared, when 0 means the bottom row.
`rowend` - The row after the last row to be cleared, default to rowstart + 1.
`return` - 0 when successed.
Erase one or multi row.

`acfcanvas.draw( x, y, width_max, str ) `
`x`, `y` - The base point for font rendering.
`width_max` - The max width of rendering the string, 0 means no limit.
`str` - The string for rendering.
`return` - nil when width is enough, or a new string for not rendered.

`acfcanvas.u8gbmp()`
`return` - lua-string.
Get the bitmap data for u8g **drawBitmap** method.

```lua
acfcanvas.setup( "somefont.acf", 256, 16 )
acfcanvas.clear()
acfcanvas.draw( 25, 2, 0, "hello,world!" )
local data = acfcanvas.u8gbmp()
disp:drawBitmap(0,0, 32, 16, data)
```
Rendering one string with the acf font.
