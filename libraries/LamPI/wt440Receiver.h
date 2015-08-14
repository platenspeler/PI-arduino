/*
 * wt440Switch library v1.2.0 (20140128) made by Randy Simons http://randysimons.nl/
 *
 * License: GPLv3. See license.txt
 */

#ifndef wt440Receiver_h
#define wt440Receiver_h

#include <Arduino.h>

#ifndef STATISTICS
#define STATISTICS 1			// if necessary, when 0 will compile without debug or statistics
#endif

struct wt440Code {
#ifdef STATISTICS
	short min1Period;			// Statistics!
	short max1Period;
	short min2Period;
	short max2Period;
#endif
	//unsigned int period;		// Detected duration in microseconds of 1T in the received signal
	byte sync;					// 4 bits 
	unsigned long address;		// 4 bits Address (house code) of received code. 
	byte channel;				// 2 bits Channel
	byte wconst;					// 3 bits
	int temperature;			//14 bits temperature (encoded)
	int humidity;				// 8 bits humidity
	byte par;					// 1 bit (Xor must be 0)
};

typedef void (*wt440ReceiverCallBack)(wt440Code);

/**
* See RemoteSwitch for introduction.
*
* wt440Receiver decodes the signal received from a 433MHz-receiver, like the "KlikAanKlikUit"-system
* as well as the signal sent by the RemoteSwtich class. When a correct signal is received,
* a user-defined callback function is called.
*
* Note that in the callback function, the interrupts are still disabled. You can enabled them, if needed.
* A call to the callback must be finished before wt440Receiver will call the callback function again, thus
* there is no re-entrant problem.
*
* When sending your own code using wt440Swich, disable() the receiver first.
*
* This is a pure static class, for simplicity and to limit memory-use.
*/

class wt440Receiver {
	public:
		/**
		* Initializes the decoder.
		*
		* If interrupt >= 0, init will register pin <interrupt> to this library.
		* If interrupt < 0, no interrupt is registered. In that case, you have to call interruptHandler()
		* yourself whenever the output of the receiver changes, or you can use InterruptChain.
		*
		* @param interrupt 	The interrupt as is used by Arduino's attachInterrupt function. See attachInterrupt for details.
							If < 0, you must call interruptHandler() yourself.
		* @param minRepeats The number of times the same code must be received in a row before the callback is called
		* @param callback Pointer to a callback function, with signature void (*func)(wt440Code)
		*/
		static void init(int8_t interrupt, byte minRepeats, wt440ReceiverCallBack callback);

		/**
		* Enable decoding. No need to call enable() after init().
		*/
		static void enable();

		/**
		* Disable decoding. You can re-enable decoding by calling enable();
		*/
		static void disable();

		/**
		* Deinitializes the decoder. Disables decoding and detaches the interrupt handler. If you want to
		* re-enable decoding, call init() again.
		*/
		static void deinit();

		/**
		* Tells wether a signal is being received. If a compatible signal is detected within the time out, isReceiving returns true.
		* Since it makes no sense to transmit while another transmitter is active, it's best to wait for isReceiving() to false.
		* By default it waits for 150ms, in which a (relative slow) KaKu signal can be broadcasted three times.
		*
		* Note: isReceiving() depends on interrupts enabled. Thus, when disabled()'ed, or when interrupts are disabled (as is
		* the case in the callback), isReceiving() will not work properly.
		*
		* @param waitMillis number of milliseconds to monitor for signal.
		* @return boolean If after waitMillis no signal was being processed, returns false. If before expiration a signal was being processed, returns true.
		*/
		static boolean isReceiving(int waitMillis = 150);

		/**
		 * Called every time the signal level changes (high to low or vice versa). Usually called by interrupt.
		 */
		static void interruptHandler();

	private:

		static int8_t _interrupt;					// Radio input interrupt
		volatile static short _state;				// State of decoding process.
		static byte _minRepeats;
		static wt440ReceiverCallBack _callback;
		static boolean _inCallback;					// When true, the callback function is being executed; prevents re-entrance.
		static boolean _enabled;					// If true, monitoring and decoding is enabled. If false, interruptHandler will return immediately.

};

#endif
