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
