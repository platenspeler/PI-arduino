/*
 * KakuTransmitter.cpp
 * based on NewRemoteSwitch library v1.2.0 (20140128) made by Randy Simons http://randysimons.nl/
 * See NewRemoteTransmitter.h for details.
 *
 * License: GPLv3. See license.txt
 */

#include "kakuTransmitter.h"


KakuTransmitter::KakuTransmitter(byte pin, unsigned int periodusec, byte repeats) {
//	_address = address;
	_pin = pin;
	_periodusec = periodusec;
	_repeats = (1 << repeats) - 1; // I.e. _repeats = 2^repeats - 1

	pinMode(_pin, OUTPUT);
}

void KakuTransmitter::sendGroup(unsigned long address, boolean switchOn) {
	for (int8_t i = _repeats; i >= 0; i--) {
		_sendStartPulse();

		_sendAddress(address);

		// Do send group bit
		_sendBit(true);

		// Switch on | off
		_sendBit(switchOn);

		// No unit. Is this actually ignored?..
		_sendUnit(0);

		_sendStopPulse();
	}
}

void KakuTransmitter::sendUnit(unsigned long address, byte unit, boolean switchOn) {
	for (int8_t i = _repeats; i >= 0; i--) {
		_sendStartPulse();

		_sendAddress(address);

		// No group bit
		_sendBit(false);

		// Switch on | off
		_sendBit(switchOn);

		_sendUnit(unit);

		_sendStopPulse();
	}
}

void KakuTransmitter::sendDim(unsigned long address, byte unit, byte dimLevel) {
	for (int8_t i = _repeats; i >= 0; i--) {
		_sendStartPulse();

		_sendAddress(address);

		// No group bit
		_sendBit(false);

		// Switch type 'dim'
		digitalWrite(_pin, HIGH);
		delayMicroseconds(_periodusec);
		digitalWrite(_pin, LOW);
		delayMicroseconds(_periodusec);
		digitalWrite(_pin, HIGH);
		delayMicroseconds(_periodusec);
		digitalWrite(_pin, LOW);
		delayMicroseconds(_periodusec);

		_sendUnit(unit);

		for (int8_t j=3; j>=0; j--) {
		   _sendBit(dimLevel & 1<<j);
		}

		_sendStopPulse();
	}
}

void KakuTransmitter::sendGroupDim(unsigned long address, byte dimLevel) {
	for (int8_t i = _repeats; i >= 0; i--) {
		_sendStartPulse();

		_sendAddress(address);

		// No group bit
		_sendBit(true);

		// Switch type 'dim'
		digitalWrite(_pin, HIGH);
		delayMicroseconds(_periodusec);
		digitalWrite(_pin, LOW);
		delayMicroseconds(_periodusec);
		digitalWrite(_pin, HIGH);
		delayMicroseconds(_periodusec);
		digitalWrite(_pin, LOW);
		delayMicroseconds(_periodusec);

		_sendUnit(0);

		for (int8_t j=3; j>=0; j--) {
		   _sendBit(dimLevel & 1<<j);
		}

		_sendStopPulse();
	}
}

void KakuTransmitter::_sendStartPulse(){
	digitalWrite(_pin, HIGH);
	delayMicroseconds(_periodusec);
	digitalWrite(_pin, LOW);
	delayMicroseconds(_periodusec * 10 + (_periodusec >> 1)); // Actually 10.5T instead of 10.44T. Close enough.
}

void KakuTransmitter::_sendAddress(unsigned long address) {
	for (int8_t i=25; i>=0; i--) {
	   _sendBit((address >> i) & 1);
	}
}

void KakuTransmitter::_sendUnit(byte unit) {
	for (int8_t i=3; i>=0; i--) {
	   _sendBit(unit & 1<<i);
	}
}

void KakuTransmitter::_sendStopPulse() {
	digitalWrite(_pin, HIGH);
	delayMicroseconds(_periodusec);
	digitalWrite(_pin, LOW);
	delayMicroseconds(_periodusec * 40);
}

void KakuTransmitter::_sendBit(boolean isBitOne) {
	if (isBitOne) {
		// Send '1'
		digitalWrite(_pin, HIGH);
		delayMicroseconds(_periodusec);
		digitalWrite(_pin, LOW);
		delayMicroseconds(_periodusec * 5);
		digitalWrite(_pin, HIGH);
		delayMicroseconds(_periodusec);
		digitalWrite(_pin, LOW);
		delayMicroseconds(_periodusec);
	} else {
		// Send '0'
		digitalWrite(_pin, HIGH);
		delayMicroseconds(_periodusec);
		digitalWrite(_pin, LOW);
		delayMicroseconds(_periodusec);
		digitalWrite(_pin, HIGH);
		delayMicroseconds(_periodusec);
		digitalWrite(_pin, LOW);
		delayMicroseconds(_periodusec * 5);
	}
}
