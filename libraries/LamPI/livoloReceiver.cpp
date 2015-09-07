/*
 * livoloSwitch library v1.2.0 (20140128) made by Randy Simons http://randysimons.nl/
 * See livoloReceiver.h for details.
 *
 * License: GPLv3. See license.txt
 */

#include "livoloReceiver.h"

#define RESET_STATE _state = -1 // Resets state to initial position.

/************
* livoloReceiver
* LIVOLO FUCTION
*
* Global Parameters Used: pulse_array, Binary_array, r_index, p_index
* p_length (as an argument)
*
* Livolo timing:
*  __
* |  |__| T + T is a 0 bit
*
*  _______
* |       | 3T is a 1 bit
* 
* Every message starts with a start pulse of 520 uSec
*
* There are 16 bits address pulses: 140+140 for a 0-bit and 300 for 1-bit
* And there are 8 bits for Device id: 140+140 or 300 uSecs
*
* Each bit is approx 300 USec long, either 140+140 or 280
* 1 start pulse
* 16 address pulses
* 7 cmd pulses

************/

int8_t livoloReceiver::_interrupt;
volatile short livoloReceiver::_state;
byte livoloReceiver::_minRepeats;
livoloReceiverCallBack livoloReceiver::_callback;
boolean livoloReceiver::_inCallback = false;
boolean livoloReceiver::_enabled = false;				// XXX Enable by default:: Brrrrr

void livoloReceiver::init(int8_t interrupt, byte minRepeats, livoloReceiverCallBack callback) {
	
	_interrupt = interrupt;
	_minRepeats = minRepeats;
	_callback = callback;
	enable();					// Hope this works
	if (_interrupt >= 0) {
		attachInterrupt(_interrupt, interruptHandler, CHANGE);
	}
}

void livoloReceiver::enable() {
	RESET_STATE;
	_enabled = true;
}

void livoloReceiver::disable() {
	_enabled = false;
}

void livoloReceiver::deinit() {
	_enabled = false;
	if (_interrupt >= 0) {
		detachInterrupt(_interrupt);
	}
}

void livoloReceiver::interruptHandler() {
	// This handler is written as one big fuction as that is the fastest
	
	if (!_enabled) {
		return;
	}

	static byte receivedBit;		// Contains "bit" currently receiving
	static livoloCode receivedCode;		// Contains received code
	static livoloCode previousCode;		// Contains previous received code
	static byte repeats = 0;		// The number of times the an identical code is received in a row.
	static unsigned long edgeTimeStamp[3] = {0, };	// Timestamp of edges
	static unsigned int min1Period, max1Period, min3Period, max3Period;

	// Allow for large error-margin. ElCheapo-hardware :(
	// Be careful, when selecting a too large max3period the receiver will "claim" action messages too.
	min1Period = 100; // Lower limit for 0 period is 0.3 times measured period; high signals can "linger" a bit sometimes, making low signals quite short.
	max1Period = 230; // Upper limit 
	min3Period = 240; // Lower limit for a 1 bit
	max3Period = 400; // Upper limit
	
	// Filter out too short pulses. This method works as a low pass filter.
	edgeTimeStamp[1] = edgeTimeStamp[2];
	edgeTimeStamp[2] = micros();

	if (_state >= 0 && 
		((edgeTimeStamp[2]-edgeTimeStamp[1] < min1Period) ||			// Filter shorts
		 (edgeTimeStamp[2]-edgeTimeStamp[1] > max3Period)) )			// Filter Long
	{
		RESET_STATE;
		return;
	}

	//unsigned int duration = edgeTimeStamp[1] - edgeTimeStamp[0];
	unsigned int duration = edgeTimeStamp[2] - edgeTimeStamp[1];
	//edgeTimeStamp[0] = edgeTimeStamp[1];

	// Note that if state>=0, duration is always >= 1 period.

	if (_state == -1) {
		// Stopbit: No stop bit (just send it 100 times)
		// By default 1T is 100µs, but for maximum compatibility go as low as 90µs
		if ((duration > 440) && (duration < 600)) { //400 is max of a normal pulse, 550 is startpulse
			// Sync signal received.. Preparing for decoding
#if STATISTICS==1
			receivedCode.min1Period = max1Period;
			receivedCode.max1Period = min1Period;
			receivedCode.min3Period = max3Period;
			receivedCode.max3Period = min3Period;
#endif
			receivedBit = 0;
			receivedCode.address= 0;
			receivedCode.unit= 0;
			receivedCode.level= 1;			// Done in direct connect sniffer also
		}
		else {
			return;
		}
	} else
		
	// If we are here, start bit recognized and we begin decoding the address
	if (_state < 32) { // As long as we are busyg with the address part 
		if (duration > min3Period) {		// 1 value for bit is 3T long
			receivedBit = 1 ;
			_state++;
			receivedCode.address = receivedCode.address *2  + receivedBit;
		}
		else {
			// We have a potential 0, if oneven _state and last read was also 0
			if ((_state % 2) == 1) {
				if (receivedBit != 0) { 
					RESET_STATE;
					return;
				}
				receivedCode.address = receivedCode.address *2 + receivedBit;
			}
			receivedBit = 0;
		}
		
	} else
	// Start with the unit code
	if (_state < 46) { // As long as we are busy with the unit part ..
		if (duration > min3Period) {		// 1 value for bit is 3T long
			receivedBit = 1 ;
			_state++;
			receivedCode.unit = receivedCode.unit *2 + receivedBit;
		}
		else {
			// We have a potential 0, if oneven _state and last read was also 0
			if ((_state % 2) == 1) {
				if (receivedBit != 0) { 
					RESET_STATE;
					return;
				}
				receivedCode.unit = receivedCode.unit *2 + receivedBit;
			}
			receivedBit = 0;
		}
	} 
	else { // Otherwise the entire sequence is invalid
		RESET_STATE;
		return;
	}
 
#if STATISTICS==1
	if (duration < max1Period) {
		if (duration < receivedCode.min1Period) receivedCode.min1Period = duration;
		else if (duration > receivedCode.max1Period) receivedCode.max1Period = duration;
	}
	else if (_state >= 0){				// Skip the start pulse of 500!
		if (duration < receivedCode.min3Period) receivedCode.min3Period = duration;
		else if (duration > receivedCode.max3Period) receivedCode.max3Period = duration;
	}
#endif

	_state++;
	   
	// OK, the complete address should be in
	if(_state == 46) {

		//Serial.print("!L "); Serial.print(_state); 
		//Serial.print(" a:"); Serial.print(receivedCode.address); 
		//Serial.print(" u:"); Serial.print(receivedCode.unit); 
		//Serial.print(" r:"); Serial.println(repeats);
		
		// a valid signal was found!
		if (
			receivedCode.address != previousCode.address ||
			receivedCode.unit != previousCode.unit ||
			receivedCode.level != previousCode.level 
		) { // memcmp isn't deemed safe
			repeats=0;
			previousCode = receivedCode;
		}
				
		repeats++;
				
		if (repeats >= _minRepeats) {
			if (!_inCallback) {
				_inCallback = true;
				(_callback)(receivedCode);
				_inCallback = false;
			}
			// Reset after callback.
			RESET_STATE;
			return;
		}
				
		// Reset for next round
		RESET_STATE;
		return;
	}
	
	return;
}

boolean livoloReceiver::isReceiving(int waitMillis) {
	unsigned long startTime=millis();

	int waited; // Signed int!
	do {
		if (_state >= 40) { // Abort if a significant part of a code (start pulse + max 14 bits) has been received
			return true;
		}
		waited = (millis()-startTime);
	} while(waited>=0 && waited <= waitMillis); // Yes, clock wraps every 50 days. And then you'd have to wait for a looooong time.

	return false;
}
