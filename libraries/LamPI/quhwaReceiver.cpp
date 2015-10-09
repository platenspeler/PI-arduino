/*
 * quhwaReceiver library v1.0 (20150510) made by M. Westenberg (mw12554@hotmail.com)
 * See quhwaReceiver.h for details.
 *
 * License: GPLv3. See license.txt
 */

#include "quhwaReceiver.h"

#define RESET_STATE _state = -1 // Resets state to initial position.

/************
* quhwaReceiver
*
* quhwa FUCTION
*
*
* quhwa timing:
*
*           _______
* Start: |_|       | (T,15T);  1 bit,   2 pulses (5800 microseconds)
*           ___   
* '0':   |_|   |     (T,3T)	; 16 bits, 32 pulses, unit: 8 bits, 16 pulse
*             _    
* '1':   |___| |     (3T,T)	; 
*
* So every message content is 36 bits and 36 pulses.
* 28 bits are used for the addresss
* 8 bits are use for the unit (constant)
*
************/

int8_t quhwaReceiver::_interrupt;
volatile short quhwaReceiver::_state;
byte quhwaReceiver::_minRepeats;
quhwaReceiverCallBack quhwaReceiver::_callback;
boolean quhwaReceiver::_inCallback = false;
boolean quhwaReceiver::_enabled = false;

void quhwaReceiver::init(int8_t interrupt, byte minRepeats, quhwaReceiverCallBack callback) {
	
	_interrupt = interrupt;
	_minRepeats = minRepeats;
	_callback = callback;
	enable();					// Hope this works
	if (_interrupt >= 0) {
		attachInterrupt(_interrupt, interruptHandler, CHANGE);
	}
}

void quhwaReceiver::enable() {
	RESET_STATE;
	_enabled = true;
}

void quhwaReceiver::disable() {
	_enabled = false;
}

void quhwaReceiver::deinit() {
	_enabled = false;
	if (_interrupt >= 0) {
		detachInterrupt(_interrupt);
	}
}

//
// The strategy is as follows: We will read pulses, and every pulse is alternating
// short or long pulse. Every bit consists of two pulses.
// We therefore expect twice as many pulses (and states) as we have bits
// We start with the
//
void quhwaReceiver::interruptHandler() {
	// This handler is written as one big fuction as that is the fastest
	
	if (!_enabled) {
		return;
	}

	static byte charcount;
	static byte receivedBit;			// Contains "bit" receiving (until changed this is previous bit)
	static quhwaCode receivedCode;		// Contains received code
	static quhwaCode previousCode;		// Contains previous received code
	static byte repeats = 0;			// The number of times the an identical code is received in a row.
	static unsigned long edgeTimeStamp[3] = {0, };	// Timestamp of edges
	
	//static unsigned int min1Period, max1Period, min3Period, max3Period;
	// What are the minimum and maximum values for 1 pulse and for 3 pulses (1)
#	define _min1Period 300
#	define _max1Period 450
#	define _min3Period 900
#	define _max3Period 1250
	
	// Filter out too short pulses. This method works as a low pass filter.
	edgeTimeStamp[1] = edgeTimeStamp[2];
	edgeTimeStamp[2] = micros();

	if (_state >= 0 && 
		((edgeTimeStamp[2]-edgeTimeStamp[1] < _min1Period) ||			// Filter shorts
		 (edgeTimeStamp[2]-edgeTimeStamp[1] > _max3Period)) )			// Filter Long
	{
		RESET_STATE;
		return;
	}
	
	//unsigned int duration = edgeTimeStamp[1] - edgeTimeStamp[0];
	unsigned int duration = edgeTimeStamp[2] - edgeTimeStamp[1];
	//edgeTimeStamp[0] = edgeTimeStamp[1];

	// Note that if state>=0, duration is always >= 1 period.

	if (_state == -1) {
		// There is no Stopbit: just send it 50 times
		if ((duration > 2000) && (duration < 7000)) { 
			// Sync signal received.. Preparing for decoding
			receivedCode.period = duration / 15;
#if STATISTICS==1
			receivedCode.min1Period = _max1Period;
			receivedCode.max1Period = _min1Period;
			receivedCode.min3Period = _max3Period;
			receivedCode.max3Period = _min3Period;
			Serial.print("-");
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
	if (_state < 27) { 							// As long as we are busy with the address part
		if (duration < _max1Period) {		// first pulse of null bit
			receivedBit = 0 ;				// Set that we expect a null bit (short-long)
#if STATISTICS==1
			if (duration < receivedCode.min1Period) receivedCode.min1Period = duration;
			if (duration > receivedCode.max1Period) receivedCode.max1Period = duration;
#endif
		}
		else {
			receivedBit = 1;				// We expect a 1 bit (long-short)
#if STATISTICS==1
			if (duration < receivedCode.min3Period) receivedCode.min3Period = duration;
			if (duration > receivedCode.max3Period) receivedCode.max3Period = duration;
#endif
		}
		receivedCode.address = receivedCode.address *2 + receivedBit;
	} else

	// Start with the unit code
	if (_state < 35) { // As long as we are busy with the unit part ..

		if (duration < _max1Period) {		// first pulse of null bit
			receivedBit = 0;				// Set that we expect a null bit (short-long)
#if STATISTICS==1
			if (duration < receivedCode.min1Period) receivedCode.min1Period = duration;
			if (duration > receivedCode.max1Period) receivedCode.max1Period = duration;
#endif
		}
		else {
			receivedBit = 1;				// We expect a 1 bit (long-short)
#if STATISTICS==1
			if (duration < receivedCode.min3Period) receivedCode.min3Period = duration;
			if (duration > receivedCode.max3Period) receivedCode.max3Period = duration;
#endif
		}
		receivedCode.unit = receivedCode.unit *2 + receivedBit;
	} else {
		RESET_STATE;
		return;
	}
	
	_state++;
	
	// OK, the complete address should be in
	if(_state == 35) {
		
		Serial.print("Address: ");
		Serial.println (receivedCode.address, BIN);
		Serial.print("Unit   : ");
		Serial.println (receivedCode.unit, BIN);		
		
		// a valid signal was found!
		if (
			receivedCode.address != previousCode.address ||
			receivedCode.unit != previousCode.unit  
		) {
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

// ----------------------------------------
//
boolean quhwaReceiver::isReceiving(int waitMillis) {
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
