/*
  wt440Transmitter, transmitter part of wt440 messages
  Created by Maarten Westenberg (mw12554 @ hotmail . com)
  
  
*/

#include "Arduino.h"
#include "wt440Transmitter.h"

#define PULSE0 2000
#define PULSE1 1000

wt440Transmitter::wt440Transmitter(byte pin)
{
  pinMode(pin, OUTPUT);
  txPin = pin;
}

/*
 * wt440Receiver
 * WT440H weather station sensor FUCTION
 *
 * timing:
 *           _   
 * '1':   |_| |     (T,T)
 *                 
 * '0':   |___|     (2T)
 * 
 * Protocol Info from: ala-paavola.fi
 *
 * bit 00-03 Leader			// 4 bits; 12 B1100
 * bit 04-07 Address		// 4 bits;
 * bit 08-09 Channel		// 2 bits;
 * bit 10-12 Constant		// 3 bits; 6 B 110
 * bit 13-20 Humidity		// 7 bits;
 * bit 21-34 Temperature 	// 15 bits; t = ( t - 6400) * 10 / 128
 * bit 35    Parity			// 1 bits
 *
 * The protocol is an FM encoded message of 36 bits. Therefore, the number of pulses
 * needed to encode the message is NOT fixed. A 0 bit is just one LONG pulse, and a
 * 1 is encoded as two pulse (alternating low-high).
 * Therefore, reading such message can be a little bit more tricky as we do not know
 * how far to read ahead is enough to have potentially received a whole message.
 * 
 * As far as I can see, every message is sent 2 times, interval is 1 minute between
 * sending new values from the sensor.
*/

void wt440Transmitter::sendMsg(wt440TxCode msgCode) {
  unsigned int temp, humi;
  
  // Retransmit 3 times to transmit a command
  for (pulse= 0; pulse <= 2; pulse = pulse+1) {
	byte par = 0;
  	high = false;	// XXX first pulse is always low-high
	
	selectPulse(1); // Start  
	selectPulse(1); // Start
	selectPulse(0); // Start
	selectPulse(0); // Start

	for (i = 4; i>0; i--) { // transmit address of 4 bits
		byte txPulse=bitRead(msgCode.address, i-1); // read bits from address
		par ^= txPulse;
		selectPulse(txPulse);    
    }

	for (i = 2; i>0; i--) { // transmit channel of 2 bits
		byte txPulse=bitRead(msgCode.channel, i-1); // read bits from keycode
		par ^= txPulse;
		selectPulse(txPulse);    
	}
	
	for (i = 3; i>0; i--) { // transmit wcode of 3 bits
		byte txPulse=bitRead(msgCode.wcode, i-1); // read bits from keycode
		par ^= txPulse;
		selectPulse(txPulse);    
	}
	// Assume parity does not change with 2 1-bit values, make const flexible to code BMP085 code
	//selectPulse(1); // Wconst 
	//selectPulse(1); // Wconst
	//selectPulse(0); // Wconst
	
	for (i = 7; i>0; i--) { // transmit humi of 8 bits
		byte txPulse=bitRead(msgCode.humi, i-1); // read bits from keycode
		par ^= txPulse;
		selectPulse(txPulse);    
	}
	
	for (i = 15; i>0; i--) { // transmit temp of 8 bits
		byte txPulse=bitRead(msgCode.temp, i-1); // read bits from keycode
		par ^= txPulse;
		selectPulse(txPulse);    
	}
	selectPulse((byte)par);
	delay(50);							// Avoid receiver pick up transmission
  }
  digitalWrite(txPin, LOW);
}

// build transmit sequence so that every high pulse is followed by low and vice versa

void wt440Transmitter::selectPulse(byte inBit) {
    switch (inBit) {
      case 0: 
		if (high == true) {   // if current pulse should be high, send High Zero
			sendPulse(2); 
		} else {              // else send Low Zero
			sendPulse(4);
		}
		high=!high; // invert next pulse
	  break;
      case 1:                // if current pulse should be high, send High One
        if (high == true) {
			sendPulse(3);			// High
			sendPulse(5);			// low
        } else {             // else send Low One
			sendPulse(5);			// low
			sendPulse(3);			// high
        }
        // high=!high; // invert next pulse not necessary for a 1 bit (2 pulses)
        break;        
	}
}

// transmit pulses
// slightly corrected pulse length, use old (commented out) values if these not working for you

void wt440Transmitter::sendPulse(byte txPulse) {

  switch(txPulse) { // transmit pulse
   case 1: // Start
   digitalWrite(txPin, HIGH);
   delayMicroseconds(1100); // Not used
   // digitalWrite(txPin, LOW); 
   break;
   case 2: // "High Zero"
   digitalWrite(txPin, LOW);
   delayMicroseconds(PULSE0); // 2000
   digitalWrite(txPin, HIGH);
   break;   
   case 3: // "High One"
   digitalWrite(txPin, LOW);
   delayMicroseconds(PULSE1); // 1000
   digitalWrite(txPin, HIGH);
   break;      
   case 4: // "Low Zero"
   digitalWrite(txPin, HIGH);
   delayMicroseconds(PULSE0); // 2000
   digitalWrite(txPin, LOW);
   break;      
   case 5: // "Low One"
   digitalWrite(txPin, HIGH);
   delayMicroseconds(PULSE1); // 1000
   digitalWrite(txPin, LOW);
   break;      
  } 
}