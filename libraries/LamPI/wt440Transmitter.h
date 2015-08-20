/*
  wt440Transmitter, transmitter part of wt440 messages
  Created by Maarten Westenberg (mw12554 @ hotmail . com)
  Released into the public domain.
*/

#ifndef wt440_h
#define wt440_h

#include "Arduino.h"
#include "LamPI.h"

// Define what we are going to send. It does NOT fit in a long so make it a struct
struct wt440Code {
		byte address;
		byte channel;
		float temp;
		float humi;
};


class wt440Transmitter
{
  public:
    wt440Transmitter(byte pin);
    void sendMsg(wt440Code msgCode);
  private:
    byte txPin;
	byte i; // just a counter
	byte pulse; // counter for command repeat
	boolean high; // pulse "sign"
	void selectPulse(byte inBit);
	void sendPulse(byte txPulse);
};

#endif