/*
 * kopouSwitch library v1.0 (20150510) made by M. Westenberg (mw12554@hotmail.com)
 * See kopouReceiver.h for details.
 *
 * License: GPLv3. See license.txt
 */

#include "kopouReceiver.h"

#define RESET_STATE _state = -1 // Resets state to initial position.

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

int8_t kopouReceiver::_interrupt;
volatile short kopouReceiver::_state;
byte kopouReceiver::_minRepeats;
kopouReceiverCallBack kopouReceiver::_callback;
boolean kopouReceiver::_inCallback = false;
boolean kopouReceiver::_enabled = false;				// XXX Enable by default:: Brrrrr

void kopouReceiver::init(int8_t interrupt, byte minRepeats, kopouReceiverCallBack callback) {
	
	_interrupt = interrupt;
	_minRepeats = minRepeats;
	_callback = callback;
	enable();					// Hope this works
	if (_interrupt >= 0) {
		attachInterrupt(_interrupt, interruptHandler, CHANGE);
	}
}

void kopouReceiver::enable() {
	RESET_STATE;
	_enabled = true;
}

void kopouReceiver::disable() {
	_enabled = false;
}

void kopouReceiver::deinit() {
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
void kopouReceiver::interruptHandler() {
	// This handler is written as one big fuction as that is the fastest
	
	if (!_enabled) {
		return;
	}

	static byte receivedBit;			// Contains "bit" receiving (until changed this is previous bit)
	static kopouCode receivedCode;		// Contains received code
	static kopouCode previousCode;		// Contains previous received code
	static byte repeats = 0;			// The number of times the an identical code is received in a row.
	static unsigned long edgeTimeStamp[3] = {0, };	// Timestamp of edges
	static unsigned int min1Period, max1Period, min2Period, max2Period;

	// Allow for large error-margin. ElCheapo-hardware :(
	// By default 1T is 120µs, but for maximum compatibility go as low as 100µs
	min1Period =  90; // Lower limit for 0 period is 0.3 times measured period; high signals can "linger" a bit sometimes, making low signals quite short.
	max1Period = 180; // Upper limit 
	min2Period = 180; // Lower limit for a 1 bit
	max2Period = 300; // Upper limit
	
	// Filter out too short pulses. This method works as a low pass filter.
	edgeTimeStamp[1] = edgeTimeStamp[2];
	edgeTimeStamp[2] = micros();

	if (_state >= 0 && 
		((edgeTimeStamp[2]-edgeTimeStamp[1] < min1Period) ||			// Filter shorts
		 (edgeTimeStamp[2]-edgeTimeStamp[1] > max2Period)) )			// Filter Long
	{
		RESET_STATE;
		return;
	}

	//unsigned int duration = edgeTimeStamp[1] - edgeTimeStamp[0];
	unsigned int duration = edgeTimeStamp[2] - edgeTimeStamp[1];
	//edgeTimeStamp[0] = edgeTimeStamp[1];

	// Note that if state>=0, duration is always >= 1 period.

	if (_state == -1) {
		// There is no Stopbit: just send it 100 times
		if ((duration > 500) && (duration < 750)) { 
			// Sync signal received.. Preparing for decoding
			receivedCode.period = duration / 5;
#ifdef STATISTICS
			receivedCode.minPeriod = duration / 5;
			receivedCode.maxPeriod = duration / 5;
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
	//if (_state == 0) {
	//	
	//} else
	// If we are here, start bit recognized and we begin decoding the address
	if (_state < 32) { // As long as we are busyg with the address part
		if ((_state % 2) == 0) {
			if (duration < max1Period) {		// first pulse of nul bit
				receivedBit = 0 ;				// Set that we expect a null bit (short-long)
			}
			else {
				receivedBit = 1;				// We expect a 1 bit (long-short)
			}
		}
		else {									// This is the second pulse
			if (duration > min2Period) {		// second pulse is short! -> 0 bit
				if (receivedBit != 0) { 
					RESET_STATE;
					return;
				}
			}
			else {								// second pulse is short -> expect a 1-bit
				if (receivedBit != 1) { 
					RESET_STATE;
					return;
				}
			}
			receivedCode.address = receivedCode.address *2 + receivedBit;
		}
	} else
	// Start with the unit code
	if (_state < 48) { // As long as we are busy with the unit part ..
		if ((_state % 2) == 0) {
			if (duration < max1Period) {		// first pulse of nul bit
				receivedBit = 0 ;				// Set that we expect a null bit (short-long)
			}
			else {
				receivedBit = 1;				// We expect a 1 bit (long-short)
			}
		}
		else {									// This is the second pulse
			if (duration > min2Period) {		// second pulse is short! -> 0 bit
				if (receivedBit != 0) { 
					RESET_STATE;
					return;
				}
#ifdef STATISTICS
				if (duration > receivedCode.maxPeriod) receivedCode.maxPeriod = duration;
#endif
				}
			else {								// second pulse is short -> expect a 1-bit
				if (receivedBit != 1) { 
					RESET_STATE;
					return;
				}
#ifdef STATISTICS
				if (duration < receivedCode.minPeriod) receivedCode.minPeriod = duration;
#endif
			}
			receivedCode.unit = receivedCode.unit *2 + receivedBit;
		}
	} 
	else { // Otherwise the entire sequence is invalid
		RESET_STATE;
		return;
	}

   _state++;
	
	// OK, the complete address should be in
	if(_state == 48) {
		
		// a valid signal was found!
		if (
			receivedCode.address != previousCode.address ||
			receivedCode.unit != previousCode.unit ||
			receivedCode.level != previousCode.level 
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

boolean kopouReceiver::isReceiving(int waitMillis) {
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
