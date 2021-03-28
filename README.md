# fastcomms
Pseudo non-blocking messaging library for Arduino, acting as an intermediary buffer between your code
and the Arduino HardwareSerial class.

Sends and receives message 'frames' terminated with an 8bit checksum (optional) and 2x MSG_END characters.

Each time FastComms::txrx() is called a single byte is read from/written to the serial port
and returns execution to the loop as soon as possible in order to achieve pseudo non-blocking
serial communication.

Default configuration can be overriden by definitions included prior to fastcomms.h

## Example usage:
```cpp
/* Default configuration in fastcomms.h:
#define BUFFER_SIZE 64 // max bytes per message
#define TX_QUEUE_SIZE 4 // max messages held in transmit queue

// FastComms will send MSG_END_A + MSG_END_B to delineate each message
// and expect the same on received messages - set Serial Monitor to 'Both NL & CR'
#define MSG_END_A '\r'
#define MSG_END_B '\n'

// error messages sent back to host 
#define RX_BUFFER_OVERFLOW "!rx buffer full!"
#define RX_BAD_CHECKSUM "!rx badchecksum!"
*/

#include <Arduino.h>
#include <fastcomms.h>

FastComms comms;

void processMsg(char *msg)
{
  // echo message back to the host
  comms.sendMsg(msg);
}

void setup()
{
  // initialise at 115200 baud, no checksums on the default Serial port
  comms.init(115200, false, &Serial); 

  // optional, message handler function to be called when a valid message is received
  // comms.setMsgHandler(processMsg);
}

void loop()
{
  // if txrx() returns true then a message is waiting
  if (comms.txrx())
  {
    processMsg(comms.getMsg()); // calling getMsg() will clear the rx buffer
  }
}
```
