/*
    FastComms - Library for non-blocking (as much as possible) serial communication for Arduino
    Created by David C. Bailey, February 29th, 2016.
    
    Sends and receives messages terminated with an 8bit checksum (optional)
    and 2x MSG_END characters
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef FastComms_h
#define FastComms_h

#include "Arduino.h"

// rx/tx buffer size in bytes
#ifndef BUFFER_SIZE
    #define BUFFER_SIZE 64
#endif

// tx command queue
#ifndef TX_QUEUE_SIZE
    #define TX_QUEUE_SIZE 4
#endif

// FastComms will send MSG_END_A + MSG_END_B and expect the same to delineate messages
#ifndef MSG_END_A
  #define MSG_END_A '\r'
#endif
#ifndef MSG_END_B
  #define MSG_END_B '\n'
#endif

// error messages sent back to host 
#ifndef RX_BUFFER_OVERFLOW
    #define RX_BUFFER_OVERFLOW "!rx buffer full!"
#endif

#ifndef RX_BAD_CHECKSUM
    #define RX_BAD_CHECKSUM "!rx badchecksum!"
#endif

class FastComms
{
    public:
        FastComms();
        
        // setup everything
        void init( const long baud, const bool useChecksum, HardwareSerial* port );
        
        // send / receive bytes, returns true if a message is waiting
        bool txrx();
        
        // allow us to set a message handler
        void setMsgHandler(const void (*msgHandler)(char* msg)); 
        
        // allow us to send a message - positive result = success, negative result = failure
        //    returns 1 
        //        on successful message send
        //    returns -1 
        //        if the command queue is full
        //    returns -2 
        //        if msg + checksum(optional) + 2xMSG_END won't fit in the buffer
        //    returns -3 
        //        if we failed to allocate memory on the heap to store the msg
        int8_t sendMsg( const char* msg );
        
        // generates a simple additive 8bit checksum for msg
        uint8_t checkSum( const char* msg );
        
        // retrieve message from message buffer, clearing it
        char* getMsg();
        
    private:
        // use checksum?
        bool _useChecksum = false;
    
        HardwareSerial* _port = nullptr;
        
        // message buffer
        char _msg[BUFFER_SIZE];
    
        // input buffer
        char _in[BUFFER_SIZE];
        
        // input buffer index
        uint8_t _i = 0;
        
        // flag goes high when we've received a msg
        bool _rx = false;
        
        // message handler pointer
        const void (*_msgHandler)(char*) = 0;
        
        
        // tx command queue - array of string pointers 
        char* _out[TX_QUEUE_SIZE];
        
        // pre allocate out buffer
        char _ob[TX_QUEUE_SIZE][BUFFER_SIZE];

        // tx buffer index
        uint8_t _obi = 0;

        // tx queue index
        uint8_t _o = 0;
        
        // the index to the next byte we'll send
        uint8_t _txb = 0;
        
        // length of message of the current message being sent
        uint8_t _txlen = 0;    
};

#endif
