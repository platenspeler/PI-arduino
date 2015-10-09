/*
  quhwaTransmitter.h defines
  Version: 1.7
  Date: Sep 23, 2015
  
  Just a few simple defines
*/

#ifndef Quhwa_h
#define Quhwa_h

#include "Arduino.h"
#include "LamPI.h"

//
// Pulse is 350 uSec, 0
// 3T is 1050uSec is a 1050uSec
// 16T is start/stop bit
//
#define QUHWA_START 6000
#define QUHWA_0 350
#define QUHWA_1 1050

class Quhwa
{
  public:
    Quhwa(byte pin);
    void sendButton(unsigned long remoteID, byte keycode);
  private:
    byte txPin;
	byte i; 				// just a counter
	byte pulse; 			// counter for command repeat
	boolean high; 			// pulse "sign"
	void selectPulse(byte inBit);
	void sendPulse(byte txPulse);
};

#endif