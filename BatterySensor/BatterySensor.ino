/*
* Code for RF remote 433 Slave Sensor (forwarding sensor reading over the air 433MHz to master). 
*
* Version 1.7.4; 151113
* (c) M. Westenberg (mw12554@hotmail.com)
*
* Connect sender no pin D8 , SDA to pin A4 and SCL to pin A5
*/

#include <Arduino.h>
#include <LamPI.h>
#include <Wire.h>
#include "BatterySensor.h"
#include "LowPower.h"
#include <wt440Transmitter.h>

// Sensors (Set in .h file)
//
#if S_DALLAS==1
#include "OneWire.h"
#include "DallasTemperature.h"
  OneWire oneWire(ONE_WIRE_BUS);
  // Pass our oneWire reference to Dallas Temperature. 
  DallasTemperature sensors(&oneWire);
  int numberOfDevices; 		// Number of temperature devices found
#endif

#if S_HTU21D==1
#include <HTU21D.h>
  HTU21D myHumidity;		// Init Sensor(s)
#endif

#if S_BMP085==1
#include <bmp085.h>
  BMP085 bmp085;
#endif

#if S_BATTERY==1
	int batteryPrevValue;	// Keep track of previous value and 
							// ONLY send a message once the battery value changes!
#endif

// Others
unsigned long time;
int debug;
int readCnt;				// Character count in buffer
char readChar;				// Last character read from tty
char readLine[32];			// Buffer of characters read
unsigned int msgCnt;
unsigned long codecs;		// must be at least 32 bits for 32 positions. Use long instead of array.

// Create a transmitter using digital pin 8 to transmit,
// with a period duration of 260ms (default), repeating the transmitted
// code 4 times. (see w440Transmitter.cpp)

wt440Transmitter wtransmitter(8);

// --------------------------------------------------------------------------------
//
void setup() {
  // if (F_CPU == 16000000) clock_prescale_set(clock_div_1);	// No effect
  
  msgCnt = 0;
  readCnt = 0;
  debug = DEBUG;
  codecs = 0;
  time = millis();
  
  Serial.begin(BAUDRATE);					// As fast as possible for USB bus 
  digitalWrite(S_TRANSMITTER, LOW);
  
#if S_DALLAS==1
	sensors.begin();
	numberOfDevices = sensors.getDeviceCount();
	if (debug>=1) {
		Serial.print("DALLAS #:");
		Serial.print(numberOfDevices); 
		Serial.println(" ");
	}
#endif

#if S_HTU21D==1
	if (debug>=1) Serial.println("HTU21D ");
	myHumidity.begin();
	myHumidity.readHumidity();			// First read value after starting does not make sense.  
#endif

#if S_BMP085==1
	if (debug>=1) Serial.println("BMP085 ");
	if (bmp085.Calibration() == 998) Serial.println(F("No bmp085"));	// OnBoard
#endif

#if S_BATTERY
   // use the 1.1 V internal reference
   analogReference(INTERNAL);
   batteryPrevValue = 0;
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
  //digitalWrite(S_TRANSMITTER, LOW);
  
// HTU21D
// The htu21d device reports its values of temp and humi as floats.
//
#if S_HTU21D==1
	// HTU21 or SHT21
	
	//delay(50);
	float humd = myHumidity.readHumidity();
	float temp = myHumidity.readTemperature();
	if (((int)temp == 999) || ((int)temp == 998)) { // 998 is timeout, 999 crc
		Serial.print(F("HTU temp error "));
		Serial.println(temp);
	}
	else
	if (((int)humd == 999) || ((int)humd == 998 )) {	// Timeout (no sensor) or CRC error
		Serial.print(F("HTU humi error "));
		Serial.println(humd);
	}
	else {
		msgCode.address = OWN_ADDRESS;
		msgCode.channel = 0;				// Fixed for HTU21D if this is a repeater
		msgCode.humi = humd;
		msgCode.temp = (unsigned int) (temp * 128) + 6400;
		msgCode.wcode = 0x6;
		wtransmitter.sendMsg(msgCode);

		if (debug>=1) {
				
			Serial.print(F("! HTU21D  Xmit: A ")); Serial.print(msgCode.address);
			Serial.print(F(", C ")); Serial.print(msgCode.channel);
			Serial.print(F(", T ")); Serial.print(msgCode.temp);
			Serial.print(F(", H ")); Serial.print(msgCode.humi);
		
			Serial.print(F(";\t\ttemp ")); Serial.print(temp, 1);
			Serial.print(F(", H ")); Serial.print(humd, 1);	
			Serial.print(F(", W ")); Serial.print(msgCode.wcode, BIN);
			
			Serial.println();
		}
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
		msgCode.humi = pressure/100 - 930;			// pressure coded as special case wt440
		msgCode.temp = (unsigned int) ((temperature * 12.8) + 6400);  // BMP085 gives temperature*10;
		msgCode.wcode = 0x7;						// Weather code a BMP085 device with airpressure
		wtransmitter.sendMsg(msgCode);
		
		if (debug>=1) {
			Serial.print(F("! BMP085  Xmit: A ")); Serial.print(msgCode.address);
			Serial.print(F(", C ")); Serial.print(msgCode.channel);
			Serial.print(F(", T ")); Serial.print(msgCode.temp);
			Serial.print(F(", H ")); Serial.print(msgCode.humi);
			
			Serial.print(F(";\t\tP ")); Serial.print(pressure/100, 2);
			Serial.print(F(", T ")); Serial.print((float)(temperature/10), 1);
			Serial.print(F(", A ")); Serial.print(altitude, 2);
			Serial.print(F(", W ")); Serial.print(msgCode.wcode, BIN);
		}
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
			msgCode.channel = i+UNIT_OFFSET;	// When present HTU21 is 0, BMP085=1, other sensors start at 2
			msgCode.humi = 0;
			msgCode.temp = (unsigned int) (tempC * 128) + 6400; 
			msgCode.wcode = 0x6;				// Weather code is standard temp/humi
			
			tRestart(200);						// XXX wakeup the transmitter with high-low pulse
			wtransmitter.sendMsg(msgCode);		// Transmit message
			
			if (debug>=1) {
				Serial.print("! DS18b20 Xmit: A "); Serial.print(msgCode.address);
				Serial.print(", C "); Serial.print(msgCode.channel);
				Serial.print(", T "); Serial.print(msgCode.temp);
				Serial.print(", H "); Serial.print(msgCode.humi);
			
				Serial.print(F(";\t\tid "));
				// Address bus is tempDeviceAddress, channel 0
				// For LamPI, print in different format compatible with WiringPI
				for (uint8_t j=0; j < 7; j++)				// Skip int[7], this is CRC
				{
					if (j<1) ind=j; else ind=7-j;
					if (j==1) Serial.print("-");
					if (tempDeviceAddress[ind] < 16) Serial.print("0");
					Serial.print(tempDeviceAddress[ind], HEX);
				}
				Serial.print(F(" T ")); 
				Serial.print(tempC, 1);
				// Serial.print(F(", W ")); 
				// Serial.print(msgCode.wcode, BIN);
				Serial.println("");
			}
			msgCnt++;
		} 
		//else ghost device! Check your power requirements and cabling
	}
#endif

  if (debug >= 1) {
	delay(100);								// NEEEDED to avoid LowPower to disable UART too early
  }
  // Wait between reading the sensors and reading battery
  // to avoid battery reading fluctuate every reading
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  
#if S_BATTERY==1
  int batteryValue = analogRead(BATTERY_PIN);
  float batteryVoltage  = batteryValue * 4.45 / 1023;
  if (batteryValue != batteryPrevValue) {
	// Battery value changed, so send a message
	msgCode.address = OWN_ADDRESS;
	// msgCode.channel = 0;					// Must be used by the last of the sensors above
	msgCode.humi = 0;
	msgCode.temp = (unsigned int) (batteryValue);
	msgCode.wcode = 0x4;					// Use a this free code for the battery
	
	tRestart(200);							// XXX Restart transmitter
	wtransmitter.sendMsg(msgCode);
	//batteryPrevValue = batteryValue;
  }
  if (debug >= 1) {
	Serial.print(" ! Battery: ");
	Serial.print(batteryValue);
	Serial.print(", V: ");
	Serial.println(batteryVoltage);
  }
#endif

  if (debug >= 1) {
	delay(100);								// NEEEDED to avoid LowPower to disable UART too early
  }

  // Do not use 60 seconds exactly to avoid all sensors reporting on the same time
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}

void tRestart(int ms) {
	int txPin = S_TRANSMITTER;
	digitalWrite(txPin, HIGH);
	delayMicroseconds(100);
	digitalWrite(txPin, LOW);
	delayMicroseconds(ms);
}

