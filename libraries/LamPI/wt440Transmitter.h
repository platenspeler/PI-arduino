/*
  wt440Transmitter, transmitter part of wt440 messages
  Created by Maarten Westenberg (mw12554 @ hotmail . com)
  Released into the public domain.
*/

#ifndef wt440_h
#define wt440_h

#include "Arduino.h"
#include "LamPI.h"

#define STATISTICS 0

#define PULSE0 2000
#define PULSE1 1050			// Arduino Pro Mini has bad timing

// Define what we are going to send. It does NOT fit in a long so make it a struct
struct wt440TxCode {
	byte address;
	byte channel;
	byte humi;
	byte wcode;				// Normally a const with value == 0x110 == 6
	unsigned int temp;
#if STATISTICS==1
	
#endif
};

class wt440Transmitter
{
  public:
    wt440Transmitter(byte pin);
    void sendMsg(wt440TxCode msgCode);
  private:
    byte txPin;
	byte i; // just a counter
	byte pulse; // counter for command repeat
	boolean high; // pulse "sign"
	void selectPulse(byte inBit);
	void sendPulse(byte txPulse);
};

#endif