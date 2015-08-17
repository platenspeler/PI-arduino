/*
* Code for RF remote 433 Slave (forwarding sensor reading over 433 to master). 
*
* Version 1.0 alpha; 150814
* (c) M. Westenberg (mw12554@hotmail.com)
*
* Based on contributions and work of many:
* 	Randy Simons (Kaku and InterruptChain)
*	SPC for Livolo
* 	and others (Wt440) etc.
*
* Connect sender no pin D8 , SDA to pin A4 and SCL to pin A5
*/

#include "LamPI.h"		// Set statistics ON (default), undefine/comment line in file if no statistics are needed

// Devices
#define KAKU    0
#define ACTION  1
#define BLOKKER 2
#define KAKUOLD 3
#define ELRO    4
#define LIVOLO  5
#define KOPOU   6

// Sensors
#define WT440  16
#define OREGON 17

// Transmitters
#include <wt440Transmitter.h>

#include <Wire.h>
#include "HTU21D.h"

// Others

unsigned long time;
int debug;
int  readCnt;				// Character count in buffer
char readChar;				// Last character read from tty
char readLine[64];			// Buffer of characters read
int msgCnt;
unsigned long codecs;		// must be at least 32 bits for 32 positions. Use long instead of array.

// Create a transmitter using digital pin 8 to transmit,
// with a period duration of 260ms (default), repeating the transmitted
// code 2^3=8 times.

wt440Transmitter wtransmitter(8);

HTU21D myHumidity;

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

  myHumidity.begin();
  time = millis();
  myHumidity.readHumidity();			// First read value after starting does not make sense.  
										// Avoid this value being sent to raspberry daemon

  // Initialize receiver on interrupt 0 (= digital pin 2), calls the callback (for example "showKakuCode")
  // after 2 identical codes have been received in a row. (thus, keep the button pressed for a moment)

  // No receivers
}

// --------------------------------------------------------------------------------
//
void loop() {
  wt440Code msgCode;
  
  float humd = myHumidity.readHumidity();
  float temp = myHumidity.readTemperature();
  if (((int)humd == 999) || ((int)humd ==998 )) return;		// Timeout (no sensor) or CRC error
  Serial.print("Time:");
  Serial.print(millis());
  Serial.print(" Temperature:");
  Serial.print(temp, 1);
  Serial.print("C");
  Serial.print(" Humidity:");
  Serial.print(humd, 1);
  Serial.print("%");
  Serial.println();
  
  msgCode.address = 3;
  msgCode.channel = 0;
  msgCode.humi = humd;
  msgCode.temp = temp;
  
  wtransmitter.sendMsg(msgCode);
  
  delay(62000);				// Wait a second (or 60)
}


// ************************* TRANSMITTER PART *************************************




// ************************* RECEIVER STUFF BELOW *********************************

// Not present for Slave Sensor node


