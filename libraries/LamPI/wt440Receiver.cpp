/*
 * wt440Switch library v1.3.0 (20150812), M. Westenberg
 * Using parts of framework by Randy Simons http://randysimons.nl/
 * Also thanks to Jaako for his work on the WT440 protocol.
 * See wt440Receiver.h for details.
 *
 * License: GPLv3. See license.txt
 */

#define RESET_STATE _state = -1 // Resets state to initial position.

#include "wt440Receiver.h"

/************
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
 *
 * PULSE defines are found in the LamPI-wt440.h include file. T=1000, 2T = 2000
 #define WT440H_MIN_SHORT 700
 #define WT440H_MAX_SHORT 1250
 #define WT440H_MIN_LONG 1700
 #define WT440H_MAX_LONG 2300

************/

int8_t wt440Receiver::_interrupt;
volatile short wt440Receiver::_state;
byte wt440Receiver::_minRepeats;
wt440ReceiverCallBack wt440Receiver::_callback;
boolean wt440Receiver::_inCallback = false;
boolean wt440Receiver::_enabled = false;

void wt440Receiver::init(int8_t interrupt, byte minRepeats, wt440ReceiverCallBack callback) {
	_interrupt = interrupt;
	_minRepeats = minRepeats;
	_callback = callback;

	enable();
	if (_interrupt >= 0) {
		attachInterrupt(_interrupt, interruptHandler, CHANGE);
	}
}

void wt440Receiver::enable() {
	RESET_STATE;
	_enabled = true;
}

void wt440Receiver::disable() {
	_enabled = false;
}

void wt440Receiver::deinit() {
	_enabled = false;
	if (_interrupt >= 0) {
		detachInterrupt(_interrupt);
	}
}

void wt440Receiver::interruptHandler() {
	// This method is written as compact code to keep it fast. While breaking up this method into more
	// methods would certainly increase the readability, it would also be much slower to execute.
	// Making calls to other methods is quite expensive on AVR. As These interrupt handlers are called
	// many times a second, calling other methods should be kept to a minimum.
	
	if (!_enabled) {
		return;
	}


	static wt440Code receivedCode;		// Contains received code
	static wt440Code previousCode;		// Contains previous received code
	static byte receivedBit;			// Contains "bit" currently receiving
	static byte repeats = 0;			// The number of times the an identical code is received in a row.
	static unsigned long edgeTimeStamp[3] = {0, };	// Timestamp of edges
	static unsigned int min1period, max1period, min2period, max2period;
	static bool skip;

	// Allow for large error-margin. ElCheapo-hardware :(
	min1period = 800; // Lower limit for 1 period is 0.3 times measured period; high signals can "linger" a bit sometimes, making low signals quite short.
	max1period = 1500; // Upper limit 
	min2period = 1500; // Lower limit 
	max2period = 2300; // Upper limit 
	
	// Filter out too short pulses. This method works as a low pass filter.
	edgeTimeStamp[1] = edgeTimeStamp[2];
	edgeTimeStamp[2] = micros();

	if (skip) {
		skip = false;
		return;
	}

	if (_state >= 0 && edgeTimeStamp[2]-edgeTimeStamp[1] < min1period) {
		// Last edge was too short.
		// Skip this edge, and the next too.
		RESET_STATE;
		return;
	}
	
	
	// unsigned int duration = edgeTimeStamp[1] - edgeTimeStamp[0];
	unsigned int duration = edgeTimeStamp[2] - edgeTimeStamp[1];
	edgeTimeStamp[0] = edgeTimeStamp[1];
	
	// Filter
	if (duration > max2period) {
		RESET_STATE;
		return;
	}

	// Note that if state>=0, duration is always >= 1 period.
	// The WT440 does not have a longer sync pulse, but sends out 4 bits as a start
	if (_state == -1) {
		// wait for the start sequence B1100 (8 short, 4 long ).
		// By default 1T is 1000µs, but for maximum compatibility go as low as 750µs

		if ((duration > 750) && (duration < 1500)){ // =750 µs, minimal time between two edges before decoding starts.
			// Sync signal, first bit received.. Preparing for decoding
			// repeats = 0;
#if STATISTICS==1
			receivedCode.min1Period = max1period;
			receivedCode.max1Period = min1period;
			receivedCode.min2Period = max2period;
			receivedCode.max2Period = min2period;
#endif
			//receivedCode.period = duration  ; 	// 
			receivedCode.sync = 1;				// First state (this one) is OK
			receivedCode.address= 0;
			receivedCode.channel= 0;
			receivedCode.wconst= 0;
			receivedCode.humidity= 0;
			receivedCode.temperature= 0;
			receivedCode.par= 0;				// First pulse is even and doe snot count
			_state++;							// State is now 0, we have started
		}
		else {
			return;
		}
	} else 
	if (_state < 4) { // Verify start bit 1 and 2, are 4 pulses!
		// Duration must be ~1T
		if (duration > max1period) {
			RESET_STATE;
			return;
		}
		if (_state % 2 == 1 ) {
			receivedCode.sync = receivedCode.sync << 1 + 1;
			receivedCode.par ^= 1;
		}
	} else 
    //	Bit 3 and 4 are 0 (long)	
	if (_state < 8) { // Verify start bit part 3 en 4 of Sync pulse
		// Duration must be 2 pulses
		if ((duration < min2period) || (duration > max2period)){
			RESET_STATE;
			return;
		}
		_state++;
		receivedCode.sync = receivedCode.sync << 1 + 0;
		receivedCode.par ^= 0;
	} else 
	// Address is 4 bits, max 8 pulses	
	if (_state < 16) { // 
		//if (receivedCode.sync != 6) { RESET_STATE; return; }

		if (duration > max1period) {
			// We have a 0
			_state++;
			receivedCode.address = receivedCode.address * 2 + 0;
			receivedCode.par ^= 0;
		}
		else { 
			if (_state % 2 == 1 ) {
				receivedCode.address = receivedCode.address * 2 + 1; 
				receivedCode.par ^= 1;				
			}
		}
	} else
	// Channel is 2 bits
	if (_state < 20) { // 
		//if (receivedCode.sync != 6) { RESET_STATE; return; }
		if (duration > max1period) {
			// We have a 0
			_state++;
			receivedBit = 0;
		}
		else { 
			receivedBit = 1; 
		}
		if (_state % 2 == 1 ) {
			receivedCode.channel = receivedCode.channel * 2 + receivedBit; 
			receivedCode.par ^= receivedBit;
		}
	} else
	// Constant 3 bits
	if (_state < 26) { // 
		if (duration > max1period) {
			_state++;
			receivedBit = 0;
		}
		else { 
			receivedBit = 1; 
		}
		if (_state % 2 == 1 ) {
			receivedCode.wconst = receivedCode.wconst * 2 + receivedBit; 
			receivedCode.par ^= 1;
		}
	} else
	// humidity 7 bits
	if (_state < 40) { 
		// If constant not equals 6, do not bother
		if (receivedCode.wconst != 6) { 
			RESET_STATE;
			return;
		}
		if (duration > max1period) {
			// We have a 0
			_state++;
			receivedBit = 0;
		}
		else { 
			receivedBit = 1; 
		}
		if (_state % 2 == 1 ) {
			receivedCode.humidity = receivedCode.humidity * 2 + receivedBit;
			receivedCode.par ^= receivedBit;
		}
	} else
	// Temperature 15 bits
	if (_state < 70) { // 
		if (duration > max1period) {
			// We have a 0
			_state++;
			receivedBit = 0;
		}
		else { 
			receivedBit = 1; 
		}
		if (_state % 2 == 1 ) {
			receivedCode.temperature = receivedCode.temperature * 2 + receivedBit; 
			receivedCode.par ^= receivedBit;
		}
	} else
	// parity is 1 bit
	if (_state < 72) { // 
		if (duration > max1period) {
			// We have a 0
			_state++;
			receivedBit = 0;
		}
		else { 
			receivedBit = 1; 
		}
		if (_state % 2 == 1 ) receivedCode.par ^= receivedBit; // No use for a 0 value
	} 
	else { // Otherwise the entire sequence is invalid
			RESET_STATE;
			return;
	}
	_state++;

#if STATISTICS==1
	// Statistics
	if (duration > max1period) {
			if (duration < receivedCode.min2Period) receivedCode.min2Period = duration;
			if (duration > receivedCode.max2Period) receivedCode.max2Period = duration;
		}
		else { 
			if (duration < receivedCode.min1Period) receivedCode.min1Period = duration;
			if (duration > receivedCode.max1Period) receivedCode.max1Period = duration;
	}
#endif	
	// Message complete, just in case we check on >72 chars too
	if (_state >= 72) {
		
		// a valid signal was found!
		if (
			receivedCode.sync != previousCode.sync ||
			receivedCode.address != previousCode.address ||
			receivedCode.channel != previousCode.channel ||
			receivedCode.temperature != previousCode.temperature ||
			receivedCode.humidity != previousCode.humidity ||
			receivedCode.par != previousCode.par 
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
		_state=0; // no need to wait for another sync-bit!
		return;
	}
	
	return;
}

boolean wt440Receiver::isReceiving(int waitMillis) {
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
