/*
* Code for RF remote 433 Repeater (forwarding sensor/receiver reading over 433 to master). 
*
* Version 1.6 alpha; 150825
* (c) M. Westenberg (mw12554@hotmail.com)
*
* Based on contributions and work of many.
*
* Connect
* - Receiver to pin D2 
* - Transmitter to pin D8
* - SDA to pin A4  
* - SCL to pin A5, 
* - 1-Wire to pin D10
*/

#include <Arduino.h>
#include <Wire.h>
#include "LamPI.h"
#include "Repeater433.h"


#include <wt440Transmitter.h>

//
// Receivers (interrupt handling)
//
#include <InterruptChain.h>
//#if S_WT440==1
#  include <wt440Receiver.h>
//#endif
//#if S_AURIOL==1
#  include <auriolReceiver.h>	// if auriolCode undefined, preprocessor will fail.
//#endif

//
// Sensors
//
#if S_DALLAS==1
# include "OneWire.h"
# include "DallasTemperature.h"
  OneWire oneWire(ONE_WIRE_BUS);
  // Pass our oneWire reference to Dallas Temperature. 
  DallasTemperature sensors(&oneWire);
  int numberOfDevices; // Number of temperature devices found
#endif
#if S_HTU21D==1
# include <HTU21D.h>
  HTU21D myHumidity;			// Init Sensor(s)
#endif
#if S_BMP085==1
# include <bmp085.h>
  BMP085 bmp085;
#endif


//
// Others
//
unsigned long time;
int debug;
int  readCnt;				// Character count in buffer
char readChar;				// Last character read from tty
char readLine[32];			// Buffer of characters read
int msgCnt;
unsigned long codecs;		// must be at least 32 bits for 32 positions. Use long instead of array.


// Create a transmitter using digital pin 8 to transmit,
// with a period duration of 260ms (default), repeating the transmitted
// code 2^3=8 times.
//
wt440Transmitter wtransmitter(8);

// --------------------------------------------------------------------------------
// SETUP
//
void setup() {
  // As fast as possible for USB bus
  Serial.begin(115200);
  msgCnt = 0;
  readCnt = 0;
  debug = 1;
  codecs = 0;
  time = millis();
  InterruptChain::setMode(0, CHANGE);
  
#if S_WT440==1
  wt440Receiver::init(-1, 1, showWt440Code);
  InterruptChain::addInterruptCallback(0, wt440Receiver::interruptHandler);
#endif
#if S_AURIOL==1
  auriolReceiver::init(-1, 2, showAuriolCode);
  InterruptChain::addInterruptCallback(0, auriolReceiver::interruptHandler);
#endif

#if S_DALLAS==1
  Serial.print("DALLAS ");
  sensors.begin();
  numberOfDevices = sensors.getDeviceCount();
  Serial.print("#: ");Serial.print(numberOfDevices); Serial.println(" ");
#endif
#if S_HTU21D==1
  Serial.println("HTU21D ");
  myHumidity.begin();
  myHumidity.readHumidity();			// First read value after starting does not make sense.  
#endif									// Avoid this value being sent to raspberry daemon
#if S_BMP085==1
	Serial.println("BMP085 ");
  if (bmp085.Calibration() == 998) Serial.println(F("No bmp085"));	// OnBoard
#endif
  // Initialize receiver on interrupt 0 (= digital pin 2), calls the callback (for example "showKakuCode")
  // after 2 identical codes have been received in a row. (thus, keep the button pressed for a moment)

  // No receivers
}

// --------------------------------------------------------------------------------
// LOOP
//
void loop() {
  wt440TxCode msgCode;

// HTU21D
// The htu21d device reports its values of temp and humi as floats.
//
#if S_HTU21D==1
	// HTU21 or SHT21
	float humd = myHumidity.readHumidity();
	float temp = myHumidity.readTemperature();
	if (((int)humd != 999) && ((int)humd !=998 )) {	// Timeout (no sensor) or CRC error
	
		Serial.print(F("! HTU21D:: A ")); Serial.print(msgCode.address);
		Serial.print(F(", C ")); Serial.print(msgCode.channel);
		Serial.print(F(", T ")); Serial.print(msgCode.temp);
		Serial.print(F(", H ")); Serial.print(msgCode.humi);

		msgCode.address = OWN_ADDRESS;
		msgCode.channel = 0;				// Fixed for HTU21D if this is a repeater
		msgCode.humi = humd;
		msgCode.temp = (unsigned int) (temp * 128) + 6400; 
		wtransmitter.sendMsg(msgCode);
		
		msgCnt++;
	}
#endif

// BMP085
// The BMP device reports temperature as shorts (degrees * 10) and airpressure as a long.
//
#if S_BMP085==1
	// BMP085 or BMP180
	short temperature = bmp085.GetTemperature();	// Temperature is factor 10 (thus integer)
	if ((temperature != 998) && (temperature != 0)){
		long pressure = bmp085.GetPressure();
		float altitude = (float)44330 * (1 - pow(((float) pressure/bmp085.p0), 0.190295));
		
		msgCode.address = OWN_ADDRESS;
		msgCode.channel = 1;
		msgCode.humi = 0;							// pressure not supported;
		msgCode.temp = (unsigned int) ((temperature * 12.8) + 6400);  // BMP085 gives temperature*10;
		wtransmitter.sendMsg(msgCode);
		
		Serial.print(F("! BMP085 Xmit : A ")); Serial.print(msgCode.address);
		Serial.print(F(", C ")); Serial.print(msgCode.channel);
		Serial.print(F(", T ")); Serial.print(msgCode.temp);
		Serial.print(F(", H ")); Serial.print(msgCode.humi);
		
		Serial.print(F(";\t\tP ")); Serial.print(pressure, DEC);
		Serial.print(F(", A ")); Serial.print(altitude, 2);
		Serial.println();
	
		msgCnt++;
	}
#endif

#if S_DALLAS==1
	uint8_t ind;
	DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address
	sensors.requestTemperatures();
	for(int i=0; i<numberOfDevices; i++)
	{
		// Search the wire for address
		if(sensors.getAddress(tempDeviceAddress, i))
		{
			float tempC = sensors.getTempC(tempDeviceAddress);
			
			msgCode.address = OWN_ADDRESS;
			msgCode.channel = i+2;				// HTU21 is 0, BMP085=1, other sensors start at 2
			msgCode.humi = 0;
			msgCode.temp = (unsigned int) (tempC * 128) + 6400; 
			wtransmitter.sendMsg(msgCode);
			
			if (debug) {
				Serial.print("! DS18b20 Xmit: A "); Serial.print(msgCode.address);
				Serial.print(", C "); Serial.print(msgCode.channel);
				Serial.print(", T "); Serial.print(msgCode.temp);
				Serial.print(", H "); Serial.print(msgCode.humi);
			
				Serial.print(F(";\t\tid "));
				// Address bus is tempDeviceAddress, channel 0
				// For LamPI, print in different format compatible with WiringPI
				for (uint8_t j= 0; j < 7; j++)				// Skip int[7], this is CRC
				{
					if (j<1) ind=j; else ind=7-j;
					if (j==1) Serial.print("-");
					if (tempDeviceAddress[ind] < 16) Serial.print("0");
					Serial.print(tempDeviceAddress[ind], HEX);
				}
				Serial.print(F(" T "));
				Serial.print(tempC);
				Serial.println();
			}
			
			msgCnt++;
		} 
		//else ghost device! Check your power requirements and cabling
	}
#endif
  delay(62000);				// Wait a second of 60 or so. 
							// Do not use 60 exactly to avoid all sensors reporting on the same time
}

// ************************* TRANSMITTER PART *************************************

// We use wtransmitter in the sensor function



// ************************* RECEIVER STUFF BELOW *********************************

// --------------------------------------------------------------------------------
// WT440
#if S_WT440==1
void showWt440Code(wt440Code receivedCode) {

	wt440TxCode msgCode;
	msgCode.address = receivedCode.address;
	msgCode.channel = receivedCode.address;
	msgCode.humi = receivedCode.humidity;
	msgCode.temp = receivedCode.temperature;
	
	wtransmitter.sendMsg(msgCode);
	
	if (debug>=1) {
		Serial.print("! WT440 Xmit:   A "); Serial.print(msgCode.address);
		Serial.print(", C "); Serial.print(msgCode.channel);
		Serial.print(", T "); Serial.print(msgCode.temp);
		Serial.print(", H "); Serial.print(msgCode.humi);
	
		Serial.print(F(";\t\tx: ")); Serial.print(receivedCode.wconst);
		Serial.print(F(", P: ")); Serial.print(receivedCode.par);

# if STATISTICS==1
		Serial.print(F(", P1: ")); Serial.print(receivedCode.min1Period);
		Serial.print(F("-")); Serial.print(receivedCode.max1Period);
		Serial.print(F(", P2: ")); Serial.print(receivedCode.min2Period);
		Serial.print(F("-")); Serial.print(receivedCode.max2Period);
# endif
		Serial.println("");
	}
	msgCnt++;
}
#endif

// --------------------------------------------------------------------------------
// AURIOL
// As the Auriol sensor will be retransmitted as a WT440 Sensor
// and Auriol address can be larger than the 2-bit WT440 addresses, we need to do a trick.
// modulus the Auriol address by 10 and take the fractional part...
//
#if S_AURIOL==1
//
void showAuriolCode(auriolCode receivedCode) {

	wt440TxCode msgCode;
	msgCode.address = (int)(receivedCode.address % 10);	// Address 138 becomes address 8 !
	msgCode.channel = receivedCode.channel;
	msgCode.humi = 0;									// humidity NOT present for Auriol sensor
	msgCode.temp = (unsigned int) ((receivedCode.temperature * 12.8) + 6400);  // AURIOL gives temperature*10;

	wtransmitter.sendMsg(msgCode);
	
	if (debug >= 1) {	
		Serial.print("! Auriol Xmit:  A "); Serial.print(msgCode.address);
		Serial.print(", C "); Serial.print(msgCode.channel);
		Serial.print(", T "); Serial.print(msgCode.temp);
		Serial.print(", H "); Serial.print(msgCode.humi);

		Serial.print(F(";\t\tCs: ")); Serial.print(receivedCode.csum);
		Serial.print(F(", N1: ")); Serial.print(receivedCode.n1,BIN);
		Serial.print(F(", N2: ")); Serial.print(receivedCode.n2,BIN);	
# if STATISTICS==1
		Serial.print(F(", P1: ")); Serial.print(receivedCode.min1Period);
		Serial.print(F("-")); Serial.print(receivedCode.max1Period);
		Serial.print(F(", P2: ")); Serial.print(receivedCode.min2Period);
		Serial.print(F("-")); Serial.print(receivedCode.max2Period);
# endif
		Serial.println("");
	}
	msgCnt++;
}
#endif


