/*
* Code for RF remote 433 Slave (forwarding sensor reading over 433 to master). 
*
* Version 1.4 alpha; 150814
* (c) M. Westenberg (mw12554@hotmail.com)
*
* Based on contributions and work of many:
* 	Randy Simons (Kaku and InterruptChain)
*	SPC for Livolo
* 	and others (Wt440) etc.
*
* Connect sender no pin D8 , SDA to pin A4 and SCL to pin A5
*/

#include <LamPI.h>
#include <wt440Transmitter.h>
#include <Wire.h>

// Sensors
#if HTU21D==1
# include <HTU21D.h>
  HTU21D myHumidity;			// Init Sensor(s)
#endif

#include <bmp085.h>
BMP085 bmp085;


// Others
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

wt440Transmitter wtransmitter(8);

// --------------------------------------------------------------------------------
//
void setup() {
  // As fast as possible for USB bus
  Serial.begin(115200);
  msgCnt = 0;
  readCnt = 0;
  debug = 0;
  codecs = 0;
  Serial.println("HTU21D Sensor Forward!");
  time = millis();
#if HTU21D==1
  myHumidity.begin();
  myHumidity.readHumidity();			// First read value after starting does not make sense.  
#endif									// Avoid this value being sent to raspberry daemon
								
  if (bmp085.Calibration() == 998) Serial.println(F("No bmp085"));	// OnBoard

  // Initialize receiver on interrupt 0 (= digital pin 2), calls the callback (for example "showKakuCode")
  // after 2 identical codes have been received in a row. (thus, keep the button pressed for a moment)

  // No receivers
}

// --------------------------------------------------------------------------------
//
void loop() {
  wt440Code msgCode;
  
#if HTU21D==1
	// HTU21 or SHT21
	float humd = myHumidity.readHumidity();
	float temp = myHumidity.readTemperature();
	if (((int)humd != 999) && ((int)humd !=998 )) {	// Timeout (no sensor) or CRC error
		Serial.print(F("< "));
		Serial.print(msgCnt);
		Serial.print(F(" 3 0 40 0 "));		// Address bus 40, channel 0
		Serial.print(temp,1);
		Serial.print(" ");
		Serial.println(humd,1);
		msgCnt++;
		msgCode.address = 3;
		msgCode.channel = 0;
		msgCode.humi = humd;
		msgCode.temp = temp;
		wtransmitter.sendMsg(msgCode);
	}
#endif	
	// BMP085 or BMP180
	short temperature = bmp085.GetTemperature();
	if ((temperature != 998) && (temperature != 0)){
		long pressure = bmp085.GetPressure();
		float altitude = (float)44330 * (1 - pow(((float) pressure/bmp085.p0), 0.190295));
		Serial.print(F("< "));
		Serial.print(msgCnt);
		Serial.print(F(" 3 0 77 0 "));		// Address bus 77, channel 0
		Serial.print(temperature, DEC);
		Serial.print(" ");
		Serial.print(pressure, DEC);
		Serial.print(" ");
		Serial.print(altitude, 2);
		Serial.println();
		msgCnt++;
		msgCode.address = 3;
		msgCode.channel = 0;
		msgCode.humi = pressure;
		msgCode.temp = temperature;
		wtransmitter.sendMsg(msgCode);
	}
  delay(62000);				// Wait a second (or 60)
}


// ************************* TRANSMITTER PART *************************************




// ************************* RECEIVER STUFF BELOW *********************************

// Not present for Slave Sensor node


