/*
* Code for RF remote 433 Slave Sensor (forwarding sensor reading over the air 433MHz to master). 
*
* Version 1.7.2; 151015
* (c) M. Westenberg (mw12554@hotmail.com)
*/

#include <ESP8266WiFi.h>
#include <string.h>
// #include <Base64.h>
#include <Wire.h>

#include <wt440Transmitter.h>
#include "ArduinoNode.h"

// Sensors (Set in .h file)
//
#if S_DALLAS==1
// # include "OneWire.h"
# include "DallasTemperature.h"
// dallas pin
# define ONE_WIRE_BUS 2
  OneWire oneWire(ONE_WIRE_BUS);
   
  DallasTemperature sensors(&oneWire);	// Pass our oneWire reference to Dallas Temperature.
  int numberOfDevices; 			// Number of temperature devices found
#endif

// Others
unsigned long time;
int debug;
unsigned int msgCnt;


// --------------------------------------------------------------------------------
//
void setup() {
  
  Serial.begin(115200);			// As fast as possible for USB bus
  msgCnt = 0;
  debug = 1;
  time = millis();
  
#if S_DALLAS==1
	sensors.begin();
	numberOfDevices = sensors.getDeviceCount();
	if (debug>=1) {
		Serial.print("DALLAS #:");
		Serial.print(numberOfDevices); 
		Serial.println(" ");
	}
#endif

}

// --------------------------------------------------------------------------------
// LOOP
//
void loop() {

	wt440TxCode msgCode;

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
			msgCode.wcode = 0x6;				// Weather code is standard temp/humi
			//wtransmitter.sendMsg(msgCode);
			
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


  // Do not use 60 seconds exactly to avoid all sensors reporting on the same time
  delay(60000);
}



