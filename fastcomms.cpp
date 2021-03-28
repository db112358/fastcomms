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

#include "Arduino.h"
#include "fastcomms.h"

FastComms::FastComms()
{
	// can't initialise serial in here
}

// initialise function to be called inside of setup()
void FastComms::init(const long baud, const bool useChecksum, HardwareSerial *port)
{

	if (port != nullptr)
	{
		_port = port;
		_port->begin(baud);
	}

	_useChecksum = useChecksum;
}

// used to set a pointer to a message handling function called on msg receipt
void FastComms::setMsgHandler(const void (*msgHandler)(char *msg))
{
	_msgHandler = msgHandler;
}

// used to send a message
int8_t FastComms::sendMsg(const char *msg)
{
	// make sure our queue isn't full
	if (_o < TX_QUEUE_SIZE)
	{
		// grab the msg length - NB l doesn't include string terminator
		uint8_t l = strlen(msg);

		// check it will fit in our buffer with space for string terminator
		// if BUFFER_SIZE was 64, and l was 64 then there's no space for null char =(
		//  whereas 63 is ok
		if (l < BUFFER_SIZE)
		{
			// hand over a pointer to a preallocated spot
			_out[_o] = _ob[_obi];

			// copy the message to the newly allocated memory location
			_out[_o][0] = '\0';
			strncat(_out[_o], msg, BUFFER_SIZE - 1);

			// advanced the queue index
			_o++;

			// advance our pre-allocated output buffer
			if (_obi < TX_QUEUE_SIZE - 1)
			{
				_obi++;
			}
			else
			{
				_obi = 0;
			}

			// message is stored in the queue!
			return 1;
		}
		else
		{
			// message won't fit in the buffer, sorry!
			return -2;
		}
	}
	else
	{
		// queue is full, sorry!
		return -1;
	}
}

// generate and return an 8bit checksum
uint8_t FastComms::checkSum(const char *msg)
{
	uint8_t sum = 0;
	size_t i = 0;
	for (i = 0; i < strlen(msg); i++)
	{
		sum = sum + msg[i];
	}
	return sum;
}

// retrieve pointer to current message buffer
char *FastComms::getMsg()
{
	return _msg;
}

// handle tx and rx, returns true if a valid message is waiting
bool FastComms::txrx()
{
	// ensure we have a pointer to serial port
	if (_port == nullptr)
		return false;

	// RX ----------------------------------------------------------------------------------------------------
	// is the rx buffer full?
	// check we haven't run out of space to put the byte
	if (_i < BUFFER_SIZE)
	{
		// check for bytes waiting
		if (_port->available() > 0)
		{
			// store the byte in our buffer
			_in[_i] = _port->read();

			// if this isn't the first byte - check for MSG_END_B
			if (_i > 0 && _in[_i] == MSG_END_B)
			{
				// was the previous byte also MSG_END_A?
				if (_in[_i - 1] == MSG_END_A)
				{
					// that's a bingo!

					// if we have _useChecksum enabled we need to verify the message
					// ensure we actually received a checksum byte as well as at least one byte of data
					if (_useChecksum && _i > 2)
					{
						// first we need to store our received checksum
						uint8_t rxsum = _in[_i - 2];

						// now we need to grab the payload
						// maximum data bytes will be buffer size - 3
						char data[BUFFER_SIZE - 3];

						// replace the checksum in the buffer to mark the end of the data payload
						_in[_i - 2] = '\0';

						// extract the data for processing
						strcpy(data, _in);

						// compare received checksum byte with checkSum()
						if (rxsum == checkSum(data))
						{
							// yaya! good msg
							// raise the _rx flag indicating a msg has arrived
							_rx = true;

							// copy the message to our message buffer
							strcpy(_msg, data);

							// call our message handler function if we have one
							if (_msgHandler != 0)
								_msgHandler(_msg);
						}
						else
						{
							// bad checksum! send a warning
							this->sendMsg(RX_BAD_CHECKSUM);
							char buf[64];
							sprintf(buf, "!%s", data);
							this->sendMsg(buf);
							char sum[10] = "";
							sprintf(sum, "!got [%d]", rxsum);
							this->sendMsg(sum);
						}

						// reset our input buffer
						_i = 0;
					}
					else
					{
						// raise the _rx flag indicating a msg has arrived
						_rx = true;

						// replace the first of the 2x MSG_END with null termination
						// 	to make it strcpy friendly
						_in[_i - 1] = '\0';

						// copy the message to our message buffer
						strcpy(_msg, _in);

						// call our message handler function if we have one
						if (_msgHandler != 0)
							_msgHandler(_msg);
					}

					// after all that definitely reset our input buffer
					_i = 0;
				}
				else
				{
					// found MSG_END however the previous byte wasn't MSG_END also
					// increment the counter ready for another incoming byte
					_i++;
				}
			}
			else
			{
				// either the first byte, or MSG_wasn't found
				// increment the counter ready for another incoming byte
				_i++;
			}
		}
	}
	else
	{
		// rx buffer overflow before MSG_END detected!
		// reset the input buffer pointer and attempt to send a warning
		_i = 0;

		this->sendMsg(RX_BUFFER_OVERFLOW);
	}
	// TX ----------------------------------------------------------------------------------------------------
	// check to see if there are messages in the queue
	if (_o > 0)
	{
		// if we haven't started transmitting this message yet
		if (_txb == 0 && _txlen == 0)
		{
			// if we're using checksums
			if (_useChecksum)
			{
				// bytes to transmit = length of the message + 1 byte for checksum + 2 bytes for MSG_END_A & B
				_txlen = strlen(_out[0]) + 3;
			}
			else
			{
				// bytes to transmit = length of the message + 2 bytes for MSG_END_A & B
				_txlen = strlen(_out[0]) + 2;
			}
		}

		// if we still have bytes left to send
		if (_txb < _txlen)
		{
			// if there is buffer space available
			if (_port->availableForWrite() > 0)
			{
				// calculate how many bytes left to send for later use
				// _txb MUST be smaller than _txlen therefore bytes_left MUST be at least 1
				uint8_t bytes_left = _txlen - _txb;

				if (_useChecksum)
				{
					// check to see if it's time to send checksum / MSG_END_A & B etc
					if (bytes_left <= 3)
					{
						if (bytes_left == 3)
						{
							// calculate and send our checksum
							_port->write(checkSum(_out[0]));
						}
						else if (bytes_left == 2)
						{
							// send MSG_END_A
							_port->write(MSG_END_A);
						}
						else if (bytes_left == 1)
						{
							// send MSG_END_B
							_port->write(MSG_END_B);
						}
					}
					else
					{
						// send a byte packing
						_port->write(_out[0][_txb]);
					}
				}
				else
				{

					// no checksums check to see if it's time to send MSG_END_A & B
					if (bytes_left <= 2)
					{
						if (bytes_left == 2)
						{
							// send MSG_END_A
							_port->write(MSG_END_A);
						}
						else if (bytes_left == 1)
						{
							// send MSG_END_B
							_port->write(MSG_END_B);
						}
					}
					else
					{
						// send a byte packing
						_port->write(_out[0][_txb]);
					}
				}

				// increment our tx byte index

				_txb++;
			}
		}
		else
		{
			// run through the queue and move each pointer to it's new position
			uint8_t _m;
			for (_m = 1; _m <= _o; _m++)
			{
				// eg _out[0] = _out[1];
				_out[_m - 1] = _out[_m];
			}

			// decrement out queue index
			_o--;

			// reset tx message length and tx byte index
			_txlen = 0;
			_txb = 0;
		}
	}

	// TX END -------------------------------------------------------------------------------------------------

	// if this txrx call resulted in a new message then clear the _rx flag and return true
	if (_rx)
	{
		_rx = false;
		return true;
	}
	// otherwise return false
	return false;
}