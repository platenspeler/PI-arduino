/*
  Kopou.cpp - Library for Kopou wireless switches.
  Created by M. Westenberg
  Version 1.0.0
  Released into the public domain.
  
*/

/** DRAFT Need work (but first fire up the Kopou swich again)**/

#include "Arduino.h"
#include "kopouTransmitter.h"

/************
* kopouReceiver
* kopou FUCTION
*
*
* Kopou timing:
*
*           _______
* Start: |_|       | (T,5T)	; 1 bit, 2 pulses
*           ___   
* '0':   |_|   |     (T,2T)	; 16 bits, 32 pulses, unit: 8 bits, 16 pulse
*             _    
* '1':   |___| |     (2T,T)	; 
*
* So every message content is 24 bits and 48 pulses.
* 
* 
* Every message starts with 2 periods start pulse: 140 + 600 uSec
* There are 16 bits address pulses: 140+260 for a 0-bit and 260+140 for 1-bit
* And there are 8 bits for Device is: 140+260 or 260+140 uSecs
*
************/

Kopou::Kopou(byte pin)
{
  pinMode(pin, OUTPUT);
  txPin = pin;
}

// keycodes #1: 0, #2: 96, #3: 120, #4: 24, #5: 80, #6: 48, #7: 108, #8: 12, #9: 72; #10: 40, #OFF: 106
// real remote IDs: 6400; 19303, 23783
// tested "virtual" remote IDs: 10550; 8500; 7400
// other IDs could work too, as long as they do not exceed 16 bit
// known issue: not all 16 bit remote ID are valid
// have not tested other buttons, but as there is dimmer control, some keycodes could be strictly system
// use: sendButton(remoteID, keycode), see example blink.ino; 


void Kopou::sendButton(unsigned int remoteID, byte keycode) {

  for (pulse= 0; pulse <= 180; pulse = pulse+1) { // how many times to transmit a command
  sendPulse(1); // Start  
  high = true; // first pulse is always high

  for (i = 16; i>0; i--) { // transmit remoteID
    byte txPulse=bitRead(remoteID, i-1); // read bits from remote ID
    selectPulse(txPulse);    
    }

  for (i = 8; i>0; i--) { // transmit keycode
    byte txPulse=bitRead(keycode, i-1); // read bits from keycode
    selectPulse(txPulse);    
    }    
  }
   digitalWrite(txPin, LOW);
}

// build transmit sequence so that every high pulse is followed by low and vice versa

void Kopou::selectPulse(byte inBit) {
  
      switch (inBit) {
      case 0: 
       for (byte ii=1; ii<3; ii++) {
        if (high == true) {   // if current pulse should be high, send High Zero
          sendPulse(2); 
        } else {              // else send Low Zero
                sendPulse(4);
        }
        high=!high; // invert next pulse
       }
        break;
      case 1:                // if current pulse should be high, send High One
        if (high == true) {
          sendPulse(3);
        } else {             // else send Low One
                sendPulse(5);
        }
        high=!high; // invert next pulse
        break;        
      }
}

// transmit pulses
// slightly corrected pulse length, use old (commented out) values if these not working for you

void Kopou::sendPulse(byte txPulse) {

  switch(txPulse) { // transmit pulse
   case 1: // Start
   digitalWrite(txPin, HIGH);
   delayMicroseconds(500); // 550
   // digitalWrite(txPin, LOW); 
   break;
   case 2: // "High Zero"
   digitalWrite(txPin, LOW);
   delayMicroseconds(100); // 110
   digitalWrite(txPin, HIGH);
   break;   
   case 3: // "High One"
   digitalWrite(txPin, LOW);
   delayMicroseconds(300); // 303
   digitalWrite(txPin, HIGH);
   break;      
   case 4: // "Low Zero"
   digitalWrite(txPin, HIGH);
   delayMicroseconds(100); // 110
   digitalWrite(txPin, LOW);
   break;      
   case 5: // "Low One"
   digitalWrite(txPin, HIGH);
   delayMicroseconds(300); // 290
   digitalWrite(txPin, LOW);
   break;      
  } 
}