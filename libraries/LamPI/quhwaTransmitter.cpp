/*
  quhwaTransmitter.cpp - Library for Quhwa wireless doorbells.
  Author: M. Westenberg
  Version: 1.7.2
  Date: Oct 13, 2015
  
  Released into the public domain. Is similar to Livolo transmitter
  
*/

#include "Arduino.h"
#include "quhwaTransmitter.h"

Quhwa::Quhwa(byte pin)
{
  pinMode(pin, OUTPUT);
  txPin = pin;
}

//
// Send the code (multiple times) for the Bell Button

void Quhwa::sendButton(unsigned long remoteID, byte keycode) {
  Serial.println();
  Serial.print("Quhwa TX g: ");
  Serial.print(remoteID);
  Serial.print(", u: ");
  Serial.println(keycode);
  // how many times to transmit a command
  for (pulse= 0; pulse <= 25; pulse++) { 
    sendPulse(1); 	// Start  
    high = true;	// first pulse is always high

    for (i = 27; i>0; i--) { // transmit remoteID
		byte txPulse=bitRead(remoteID, i-1); // read bits from remote ID
		selectPulse(txPulse);
    }

    for (i = 8; i>0; i--) { // transmit keycode
		byte txPulse=bitRead(keycode, i-1); // read bits from keycode
		selectPulse(txPulse); 	
    }
	
	// After last pulse, change
	if (high)
		digitalWrite(txPin, HIGH);
	else
		digitalWrite(txPin, LOW);
  }
}

// build transmit sequence so that every high pulse is followed by low and vice versa

void Quhwa::selectPulse(byte inBit) {
    switch (inBit) {
      case 0: 
        if (high == true) {		// if current pulse should be high, send High Zero
			sendPulse(4); 
        } else {        		// else send Low Zero
			sendPulse(2);
        }
        high=!high; 			// invert next pulse
      break;
      case 1:          			// if current pulse should be high, send High One
        if (high == true) {
			sendPulse(5);
        } else {				// else send Low One
			sendPulse(3);
        }
        high=!high;				// invert next pulse
      break;        
    }
}

// transmit pulses
// slightly corrected pulse length, use old (commented out) values if these not working for you

void Quhwa::sendPulse(byte txPulse) {
  switch(txPulse) { // transmit pulse
   case 1:							// Start
   digitalWrite(txPin, HIGH);
   delayMicroseconds(350);
   digitalWrite(txPin, LOW);
   delayMicroseconds(6000); 		// 5800
   digitalWrite(txPin, HIGH); 
   break;
   case 2:							// "Low Zero"
   digitalWrite(txPin, LOW);
   delayMicroseconds(350); 			// 350
   digitalWrite(txPin, HIGH);
   break;
   case 3:							// "Low One"
   digitalWrite(txPin, LOW);
   delayMicroseconds(1050); 		// 1050
   digitalWrite(txPin, HIGH);
   break;
   case 4:							// "High Zero"
   digitalWrite(txPin, HIGH);
   delayMicroseconds(350); 			// 350
   digitalWrite(txPin, LOW);
   break;      
   case 5:							// "High One"
   digitalWrite(txPin, HIGH);
   delayMicroseconds(1050); 		// 1050
   digitalWrite(txPin, LOW);
   break;      
  } 
}