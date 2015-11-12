/*
* Code for RF remote 433 PIR Sensor (forwarding sensor readings over the air 433MHz to master). 
*
* Version 1.7.4; 151110
* (c) M. Westenberg (mw12554@hotmail.com)
*
* Based on contributions and work of many:
* 	Randy Simons (Kaku and InterruptChain)
*	SPC for Livolo
* 	and others (Wt440) etc.
*
* Connect sender no pin D8 , PIR to pin D6
*/


#include "ArduinoPir.h"
#include <Arduino.h>
#include <LamPI.h>
#include <Wire.h>
#include <wt440Transmitter.h>

// Others
int debug;
int msgCnt;
unsigned long time;

// Create a transmitter using digital pin 8 to transmit,
// with a period duration of 260ms (default), repeating the transmitted
// code 4 times.
wt440Transmitter wtransmitter(8);

// --------------------------------------------------------------------------------
//
void setup() {
  // As fast as possible for USB bus
  Serial.begin(BAUDRATE);
  msgCnt = 0;
  debug = 1;
  time = millis();
  digitalWrite(S_TRANSMITTER, LOW);		// Make pin low.
	

  // Initialize receiver on interrupt 0 (= digital pin 2), calls the callback (for example "showKakuCode")
  // after 2 identical codes have been received in a row. (thus, keep the button pressed for a moment)

  // No receivers
}

// --------------------------------------------------------------------------------
// LOOP
//
void loop() {
  wt440TxCode msgCode;
  short parameter1;
  long parameter2 = 0;

  parameter1 = digitalRead(PIR_PIN);			// Sensor PIR
  if (parameter1 == 1) {
	Serial.println("PIR ON");
	
	msgCode.address = OWN_ADDRESS;
	msgCode.channel = 1;
	msgCode.humi = parameter2;					// humi field not used for PIR
	msgCode.temp = (unsigned int) parameter1;	// PIR value coded as special case
	msgCode.wcode = 0x0;						// Wcode a PIR device
	wtransmitter.sendMsg(msgCode);	
	if (debug>=1) {
			Serial.print(F("! PIR  Xmit: A ")); Serial.print(msgCode.address);
			Serial.print(F(", C ")); Serial.print(msgCode.channel);
			Serial.print(F(", Val ")); Serial.print(msgCode.temp);
			Serial.print(F(", H ")); Serial.print(msgCode.humi);
			Serial.print(F(";\t\t ")); 
			Serial.print(F(", W ")); Serial.print(msgCode.wcode, BIN);
			Serial.println();
	}
	msgCnt++;
	delay(30000);								// If we detect movement, make timeout 30 seconds total
												// to avoid daemon overload.
	// XXX In a later version we need daemon confirmaton of the receipt of the alarm!
  }
  else {
	Serial.println("PIR OFF");
	delay(1000);								// Wait a second
  }

 
}


// ************************* TRANSMITTER PART *************************************

// Not used in Sensor Mode


// ************************* RECEIVER STUFF BELOW *********************************

// Not present for Slave Sensor node


