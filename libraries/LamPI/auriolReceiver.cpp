/*
 * auriol Weather Sensor library v1.3.0 (20150812), M. Westenberg
 * Using parts of framework by Randy Simons http://randysimons.nl/
 * Also thanks to TFD and others for work on the Auriol protocol.
 * See auriolReceiver.h for details.
 *
 * License: GPLv3. See license.txt
 */

#define RESET_STATE _state = -1 // Resets state to initial position.

#include "auriolReceiver.h"

/************
* auriolReceiver
 * Auriol weather station sensor FUCTION. Auriol are cheap weather stations sold by LIDL in NL.
 *
 * timing:
 *         _
 * Sync:  | |_________________| (T, 18T)
 *         _   
 * '1':   | |________|          (T,8T)
 *         _        
 * '0':   | |____|              (T, 4T)
 * 
 * Protocol Info , T= 0,5 ms, 500uSec
 *
 * bit 00-07 ID				// 8 bits;
 * bit 08-08 Battery		// 1 bits;
 * bit 09-11 <unknown>		// 3 bits
 * bit 12-23 Temperature	// 12 bits;
 * bit 24-27 <unknown 		// 4 bits; 
 * bit 28-30 <unknown>
 * bit 31-31 Parity check	// 1 bit
 *
 *
 * As far as I can see, every message is sent 6 times, interval is 1 minute between (or less)
 * sending new values from the sensor.
 *
 * PULSE defines 
 #define AURIOL_MIN_SHORT 1600
 #define AURIOL_MAX_SHORT 2400
 #define AURIOL_MIN_LONG 3600
 #define AURIOL_MAX_LONG 4400

************/

int8_t auriolReceiver::_interrupt;
volatile short auriolReceiver::_state;
byte auriolReceiver::_minRepeats;
auriolReceiverCallBack auriolReceiver::_callback;
boolean auriolReceiver::_inCallback = false;
boolean auriolReceiver::_enabled = false;

void auriolReceiver::init(int8_t interrupt, byte minRepeats, auriolReceiverCallBack callback) {
	_interrupt = interrupt;
	_minRepeats = minRepeats;
	_callback = callback;

	enable();
	if (_interrupt >= 0) {
		attachInterrupt(_interrupt, interruptHandler, CHANGE);
	}
}

void auriolReceiver::enable() {
	RESET_STATE;
	_enabled = true;
}

void auriolReceiver::disable() {
	_enabled = false;
}

void auriolReceiver::deinit() {
	_enabled = false;
	if (_interrupt >= 0) {
		detachInterrupt(_interrupt);
	}
}

void auriolReceiver::interruptHandler() {
	// This method is written as compact code to keep it fast. While breaking up this method into more
	// methods would certainly increase the readability, it would also be much slower to execute.
	// Making calls to other methods is quite expensive on AVR. As These interrupt handlers are called
	// many times a second, calling other methods should be kept to a minimum.
	
	if (!_enabled) {
		return;
	}

	static byte receivedBit;			// Contains "bit" currently receiving
	static auriolCode receivedCode;		// Contains received code
	static auriolCode previousCode;		// Contains previous received code
	static byte repeats = 0;			// The number of times the an identical code is received in a row.
	static unsigned long edgeTimeStamp[3] = {0, };	// Timestamp of edges
	static unsigned int min1period, max1period, min2period, max2period;

	// Allow for large error-margin. ElCheapo-hardware :(
	min1period = 1800;	// Lower limit for 1 period is 0.3 times measured period; 
						// high signals can "linger" a bit sometimes, making low signals quite short.
	max1period = 2200;	// Upper limit 
	min2period = 3800;	// Lower limit 
	max2period = 4200;	// Upper limit 
	
	// Filter out too short pulses. This method works as a low pass filter.
	edgeTimeStamp[1] = edgeTimeStamp[2];
	edgeTimeStamp[2] = micros();

	unsigned int duration = edgeTimeStamp[2] - edgeTimeStamp[1];
	edgeTimeStamp[0] = edgeTimeStamp[1];
	
	// Note that if state>=0, duration is always >= 1 period.
	// The auriol does have a longer sync pulse, and sends out 4 bits as a start
	if (_state == -1) {
		// By default 1T is 500µs, but for maximum compatibility go as low as 400µs

		if ((duration > 8500) && (duration < 10000)){ // =9 ms, minimal time between two edges before decoding starts.
			// Sync signal, first pulse received.. Preparing for decoding
#if STATISTICS==1
			receivedCode.min1Period = max1period;
			receivedCode.max1Period = min1period;
			receivedCode.min2Period = max2period;
			receivedCode.max2Period = min2period;
#endif
			//receivedCode.period = duration  ; 	// 
			receivedCode.address= 0;
			receivedCode.n1 = 0;
			receivedCode.n2 = 0;
			receivedCode.channel= 0;
			receivedCode.humidity= 0;
			receivedCode.temperature= 0;
			receivedCode.csum= 0;				// First pulse is even and does not count
			//_state++;							// State is now 0, we have started
		}
		else {
			return;
		}
	} else
	// Do this check for every even _state
	if (_state % 2 == 0){
		if ((duration < 350 ) || (duration > 700)) {
			RESET_STATE;
			return;
		}
		// Else we increase _state below
	} else
	// Must be an odd pulse. These are longer but within certain boundaries
	if ( (duration < min1period) ||  (duration > max2period) 
		|| ((duration > max1period) && (duration < min2period)) ){
		RESET_STATE;
		return;
	} else
		
	// Begin of an address
	if (_state < 16) { // Verify start address 0-7, are 16 pulses! Random ID
		receivedCode.address = receivedCode.address *2;
		if (duration > min2period) {
			receivedCode.address +=1;
			receivedCode.csum ^= 1;
		}
	} else 
    //	Bits 8-11 are n2  bits. MSB is the battery bit
	if (_state < 24) { 
		receivedCode.n1 = receivedCode.n1 *2;
		if (duration > min2period) {
			receivedCode.n1 += 1;
			receivedCode.csum ^= 1;
		}
	} else 
	// Temperature is 12 bits, max 24 pulses	
	if (_state < 48) { // 
		receivedCode.temperature = receivedCode.temperature * 2;
		if (duration > min2period) {
			receivedCode.temperature += 1;
			receivedCode.csum ^= 1;
		}
	} else
	// Unknown use for next 7 bits, 14 pulses
	if (_state < 62) {		// 
		receivedCode.n2 = receivedCode.n2 *2;
		if (duration > min2period) {
			receivedCode.n2 += 1;
			receivedCode.csum ^= 1;
		}
	} else
	// Checksum 1 bits, 2 pulses, Actuall last bit used for parity (X-or all bits)
	if (_state < 64) { // 
		if (duration > min2period) {	// If a 1-bit	
			receivedCode.csum ^= 1;		// So the result must always be 0
		}
	} else 
	// Invalid
	{
		RESET_STATE;
		return;
	}
	
#if STATISTICS==1
	// Statistics
	if (_state % 2 == 1) {
		if (duration > max1period) {
			if (duration < receivedCode.min2Period) receivedCode.min2Period = duration;
			if (duration > receivedCode.max2Period) receivedCode.max2Period = duration;
		}
		else { 
			if (duration < receivedCode.min1Period) receivedCode.min1Period = duration;
			if (duration > receivedCode.max1Period) receivedCode.max1Period = duration;
		}
	}
#endif	

	_state++;
	
	// Message complete
	if (_state == 64) {
		
		// a valid signal was found!
		if (
			receivedCode.address != previousCode.address ||
			receivedCode.channel != previousCode.channel ||
			receivedCode.temperature != previousCode.temperature ||
			receivedCode.humidity != previousCode.humidity ||
			receivedCode.csum != previousCode.csum 
			) { // memcmp isn't deemed safe
					repeats=0;
					previousCode = receivedCode;
		}
				
		repeats++;
				
		if (repeats>=_minRepeats) {
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

boolean auriolReceiver::isReceiving(int waitMillis) {
	unsigned long startTime=millis();

	int waited; // Signed int!
	do {
		if (_state >= 30) { // Abort if a significant part of a code (start pulse + 8 bits) has been received
			return true;
		}
		waited = (millis()-startTime);
	} while(waited>=0 && waited <= waitMillis); // Yes, clock wraps every 50 days. And then you'd have to wait for a looooong time.

	return false;
}
