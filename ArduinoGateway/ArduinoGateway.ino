/*
* Code for RF remote switch Gateway. 
*
* (c) M. Westenberg (mw12554@hotmail.com)
*
* Based on contributions and work of many:
* 	Randy Simons (Kaku and InterruptChain)
*	Spc for Livolo
*	SPARCfun for HTU21 code and BMP85
* 	and others (Wt440 protocol) etc.
*
* Connect the receiver to digital pin 2, sender to pin 8. Other definitions in LamPI.h
* NOTE: You can enable/disable modules in the LamPI.h file
*		Sorry for making the code in this file so messy :-) with #if
*/

/*
  This file implements the Arduino side of the LamPI Gateway.
  The gate way can be used stand-alone and be controlled through the Serial port or can be
  - as designed - work together with the LamPI-arduino process on the Raspberry server.
  
  Please keep in mind that all protocols that are implemented on the Arduino side need to be
  supported by the LamPI-arduino process on the Raspberry as well in order to provide
  reliable and meaningful communication.
  So: baudrate and supported potocolmust be set corecly in the ArduinoGateway.h file
  And these must match the lampi_arduino.h definitions in file on ~/receivers/arduino on Raspberry
*/

// Please look at http://platenspeler.github.io/HardwareGuide/Arduino-Gateway/gatewayMsgFormat.html
// for definitions of the message format for the Arduino Gateway.
// Command "> 111 0 4" will output the version of the Gateway software
#define VERSION " ! V. 1.7.4, 151104"

#include "ArduinoGateway.h"

// General Include
//
#include <Arduino.h>
#include <Wire.h>
#include "LamPI.h"				// Shared Definitions, such as PIN definitions
#include <Streaming.h>          //http://arduiniana.org/libraries/streaming/


// Transmitters Include
// Be careful not to include transmitters that initialize structures in their .h file
// which will negatively impact the memory
//
#include <kakuTransmitter.h>
#include <RemoteTransmitter.h>
#include <livoloTransmitter.h>
#include <kopouTransmitter.h>
#include <quhwaTransmitter.h>
//
// Receivers include. 
// Some cannot be commented as the preprocessor need the argument type of functions
// that are defined inside an #if (for unknown reasons, complex parameters in functions will then not be recognized)
//
#include <wt440Receiver.h>
#include <kakuReceiver.h>
#include <livoloReceiver.h>
#include <RemoteReceiver.h>
#include <kopouReceiver.h>
#include <RemoteReceiver.h>
#include <auriolReceiver.h>		// if auriolCode undefined, preprocessor will fail.
#include <quhwaReceiver.h>
#include <InterruptChain.h>

//
// Sensors Include
//
#if S_DALLAS==1
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

#if S_HTU21D==1
#include <HTU21D.h>
#endif

#if S_BMP085==1
#include <bmp085.h>
#endif

#if S_BH1750==1
#include <BH1750FVI.h>
#endif

#if S_DS3231==1
#include <Time.h>          //http://playground.arduino.cc/Code/Time
#include "DS3232RTC.h"		//http://github.com/JChristensen/DS3232RTC
#endif

unsigned long time;			// fill up with millis();
boolean debug;				// If set, more informtion is output to the serial port
int  readCnt;				// Character count in buffer
char readChar;				// Last character read from tty
char readLine[32];			// Buffer of characters read
unsigned int msgCnt;		// Not unique, as at some time number will wrap.

// Use a bit array (coded in long) to keep track of what protocol is enabled.
// Best is to make this dynamic and not compile time. However,only the Arduino Mega has enough memory
// to run all codecs with all their declarations without problems.
//
unsigned long codecs;		// must be at least 32 bits for 32 positions. Use long instead of array.


// Sensors Declarations
#if S_DALLAS==1
  OneWire oneWire(ONE_WIRE_BUS);	// pin ONE_WIRE_BUS is set in LamPI.h
  // Pass our oneWire reference to Dallas Temperature. 
  DallasTemperature sensors(&oneWire);
  int numberOfDevices; 		// Number of temperature devices found
#endif
#if S_HTU21D==1
  HTU21D myHumidity;		// Init Sensor(s)
#endif
#if S_BMP085==1
  BMP085 bmp085;
#endif
#if S_BH1750==1
  BH1750FVI LightSensor;
#endif

// --------------------------------------------------------------------------------
//
void setup() {
  
	Serial.begin(BAUDRATE);			// As fast as possible for USB bus, defined in Transeiver433.h
	msgCnt = 0;
	readCnt = 0;
	debug = DEBUG;
	codecs = 0;

#if A_MEGA==1
	Serial.println(F("! Mega Gateway"));
	Serial.print(F("! debug: ")); Serial.println(debug);
#endif

  // Create a transmitter using digital pin S_TRANSMITTER (default is 8) to transmit,
  // with a period duration of 260ms (default), repeating the transmitted
  // code 2^3=8 times.
  digitalWrite(S_TRANSMITTER, LOW);

#if S_DS3231==1
	setSyncProvider(RTC.get);
	Serial << F("! RTC Sync");
    if (timeStatus() != timeSet) Serial << F(" FAIL!"); else Serial << F(" OK");
    Serial << endl;
	onCodec(ONBOARD);
#endif
  
	// Initialize receiver on interrupt 0 (= digital pin 2), calls the callback (for example "showKakuCode")
	// after 2 identical codes have been received in a row. (thus, keep the button pressed for a moment)

	KakuReceiver::init(-1, 2, showKakuCode);
	RemoteReceiver::init(-1, 2, showRemoteCode);
#if R_LIVOLO
	livoloReceiver::init(-1, 3, showLivoloCode);
#endif
#if R_KOPOU==1
	kopouReceiver::init(-1, 3, showKopouCode);
#endif
#if R_QUHWA==1
	quhwaReceiver::init(-1, 3, showQuhwaCode);
#endif
#if S_WT440==1
	wt440Receiver::init(-1, 1, showWt440Code);
#endif
#if S_AURIOL==1
	auriolReceiver::init(-1, 2, showAuriolCode);
#endif

  // Change interrupt mode to CHANGE (on flanks). First argument is interrupt number
  InterruptChain::setMode(0, CHANGE);
  
  // Define the interrupt chain
  // The sequence might be relevant, defines the order of execution, put easy protocols first
  // onCodec: First 16 bits are for sensors (+16), last 16 bits for devices,
  // By removing a line below, that type of device will not be scanned anymore :-)
  //

#if S_AURIOL==1
	InterruptChain::addInterruptCallback(0, auriolReceiver::interruptHandler);	onCodec(AURIOL);
#endif
	InterruptChain::addInterruptCallback(0, RemoteReceiver::interruptHandler);	onCodec(ACTION);
	InterruptChain::addInterruptCallback(0, KakuReceiver::interruptHandler);	onCodec(KAKU);
	InterruptChain::addInterruptCallback(0, livoloReceiver::interruptHandler);	onCodec(LIVOLO);
#if R_KOPOU==1
	InterruptChain::addInterruptCallback(0, kopouReceiver::interruptHandler);	onCodec(KOPOU);
#endif
#if R_QUHWA==1
	InterruptChain::addInterruptCallback(0, quhwaReceiver::interruptHandler);	onCodec(QUHWA);
#endif
#if S_WT440==1
	InterruptChain::addInterruptCallback(0, wt440Receiver::interruptHandler);	onCodec(WT440);
#endif

	time = millis();

#if S_DALLAS==1
	sensors.begin();
	onCodec(ONBOARD);
	numberOfDevices = sensors.getDeviceCount();
	Serial.print("! #Dallas: "); Serial.println(numberOfDevices);
#endif
#if S_HTU21D==1
	myHumidity.begin(); 
	myHumidity.setResolution(0);
	onCodec(ONBOARD);
	if (myHumidity.readHumidity() == 998) Serial.println(F("! No HTU21D")); // First read value after starting does not make sense.  
#endif
#if S_BMP085==1
	bmp085.begin(); 
	onCodec(ONBOARD);
	if (bmp085.Calibration() == 998) Serial.println(F("! No BMP085"));	// OnBoard
#endif
#if S_BH1750==1
	LightSensor.begin();
	onCodec(ONBOARD);
	LightSensor.SetAddress(Device_Address_H);							//Address 0x23 or 0x5C
	LightSensor.SetMode(Continuous_H_resolution_Mode2);
	if (LightSensor.GetLightIntensity() == (int) -1) {
		Serial.println(F("! No BH1750"));
	};
#endif
#if A_MEGA==1
	printCodecs();
	if (debug==1) {
		Serial.print(F("Pin 2 is interrupt: "));
		Serial.println(digitalPinToInterrupt(2));
	}
#endif
}

// --------------------------------------------------------------------------------
//
void loop() {
  char * pch;
  // Must make sure digital transmitter pin is low when not used
  digitalWrite(S_TRANSMITTER, LOW);
  
  while (Serial.available()) {
    InterruptChain::disable(0);					// Set interrupts off  
	readChar = Serial.read();					// Read the requested byte from serial
	if (readChar == '\n') {						// If there is a newLine in the input
	  readLine[readCnt]='\0';	  				// Overwrite the \n char, close the string
	  parseCmd(readLine);						// ACTION: Parsing the readLine for actions
	  readLine[0]='\0';
	  readCnt=0;  
	}
	else {
	  readLine[readCnt]=readChar;
	  readCnt++;
	  readLine[readCnt]='\0';
	}
	InterruptChain::enable(0);					// Set interrupts on again
  }//available

  readSensors();								// Better for callbacks if there are sleeps
}

// ***************************** Codecs Admin *************************************

//
// Switch a codec to on.
//
void onCodec (byte codec) {
	codecs |= ( (unsigned long) 0x0001 << codec	);		// AND
#if A_MEGA==1
  // We support dynamic enablement and disable of sensors and devices
  
#endif
}

//
// We really should disable interrupts for codecs that are in the interrupt chain too.
//
void offCodec (byte codec) {
	codecs &= ~( (unsigned long) 0x0001 << codec );		// NAND, no effect for 0, but for 1 bit makes 0
#if A_MEGA==1
	// We support dynamic enablement and disable of sensors and devices
	
#endif
}

void setCodec (byte codec, byte val) {
  if (val = 0) offCodec(codec);
  else onCodec(codec);
}

#if A_MEGA==1
void printCodecs() {
	Serial.print(F("! Codecs enabled: "));
	if ((codecs >> KAKU) & 0x0001) Serial.print(F("KAKU "));
	if ((codecs >> ACTION) & 0x0001) Serial.print(F("ACTION "));
	if ((codecs >> BLOKKER) & 0x0001) Serial.print(F("BLOKKER "));
	if ((codecs >> KAKUOLD) & 0x0001) Serial.print(F("KAKOLD "));
	if ((codecs >> ELRO) & 0x0001) Serial.print(F("ELRO "));
	if ((codecs >> KOPOU) & 0x0001) Serial.print(F("KOPOU "));
	if ((codecs >> LIVOLO) & 0x0001) Serial.print(F("LIVOLO "));
	if ((codecs >> QUHWA) & 0x0001) Serial.print(F("QUWAH "));
	
	if ((codecs >> ONBOARD) & 0x0001) Serial.print(F("ONBOARD "));
	if ((codecs >> WT440) & 0x0001) Serial.print(F("WT440 "));
	if ((codecs >> OREGON) & 0x0001) Serial.print(F("OREGON "));
	if ((codecs >> AURIOL) & 0x0001) Serial.print(F("AURIOL "));
	if ((codecs >> CRESTA) & 0x0001) Serial.print(F("CRESTA "));
	Serial.println();
}
#endif

// ************************* SENSORS PART *****************************************


// --------------------------------------------------------------------------------
// 
void readSensors() {
	if ((millis() - time) > 63000) {				// 63 seconds, so avoiding collisions with other cron jobs
		time = millis();
#if S_DALLAS
		uint8_t ind;
		DeviceAddress tempDeviceAddress; 			// We'll use this variable to store a found device address
		sensors.requestTemperatures();
		for(int i=0; i<numberOfDevices; i++)
		{
			// Search the wire for address
			if(sensors.getAddress(tempDeviceAddress, i))
			{
					
				Serial.print(F("< "));
				Serial.print(msgCnt++);
				Serial.print(F(" 3 0 "));				// Internal Sensor
				
				// Address bus is tempDeviceAddress, channel 0
				// For LamPI, print in different format compatible with WiringPI
				for (uint8_t j= 0; j < 7; j++)			// Skip int[7], this is CRC
				{
					if (j<1) ind=j; else ind=7-j;
					if (j==1) Serial.print("-");
					if (tempDeviceAddress[ind] < 16) Serial.print("0");
					Serial.print(tempDeviceAddress[ind], HEX);
				}
				Serial.print (F(" 0 "));				// Channel
				// It responds almost immediately. Let's print out the data
				float tempC = sensors.getTempC(tempDeviceAddress);
				Serial.print(tempC,1);
				Serial.print(F(" 0"));					// No Humidity, make 0
				
				// Output the device ID
				if (debug) {
					Serial.print(" ! ds18b20 dev ");
					Serial.print(i, DEC);
				}
				Serial.println();
			} 
			//else ghost device! Check your power requirements and cabling
		}
#endif

#if S_HTU21D==1
		// HTU21 or SHT21
		//delay(50);
		float humd = myHumidity.readHumidity();
		float temp = myHumidity.readTemperature();
		if (((int)humd != 999) && ((int)humd != 998 )) {	// Timeout (no sensor) or CRC error
			Serial.print(F("< "));
			Serial.print(msgCnt);
			Serial.print(F(" 3 0 40 0 "));		// Address bus 40, channel 0
			Serial.print(temp,1);
			Serial.print(F(" "));
			Serial.print(humd,1);
			if (debug) {
				Serial.print(F(" ! HTU21 T: "));
				Serial.print(temp/10);
			}
			Serial.println();
			msgCnt++;
		}
		else if (debug>=1) Serial.println(F(" ! No HTU21"));
#endif
#if S_BMP085==1		
		// BMP085 or BMP180
		short temperature = bmp085.GetTemperature();
		if ((temperature != 998) && (temperature != 0)){
			long pressure = bmp085.GetPressure();
			float altitude = (float)44330 * (1 - pow(((float) pressure/bmp085.p0), 0.190295));
			Serial.print(F("< "));
			Serial.print(msgCnt);
			Serial.print(F(" 3 0 77 0 "));					// Address bus 77, channel 0
			Serial.print(temperature, DEC);
			Serial.print(F(" "));
			Serial.print(pressure, DEC);
			Serial.print(F(" "));
			Serial.print(altitude, 2);
			if (debug>=1) {
				Serial.print(F(" ! bmp: t: "));
				Serial.print((float)(temperature/10), 1);
			}
			Serial.println();
			msgCnt++;
		}
#endif
#if S_BH1750==1
		uint16_t lux = LightSensor.GetLightIntensity();		// Get Lux value
		if (lux != (int) -1) {
			Serial.print(F("< "));
			Serial.print(msgCnt);
			Serial.print(F(" 3 0 23 0 "));						// Address 23 or 5C
			Serial.print(lux);
			if (debug>=1) {
				Serial.print(F(" ! Lumi: "));
				Serial.print(lux);
				Serial.print(F(" lux"));
			}
			Serial.println();
			msgCnt++;
		}
#endif
	}
}


// ************************* TRANSMITTER PART *************************************

// --------------------------------------------------------------------------------
// PARSE COMMAND (first part of message)
//
void parseCmd(char *readLine) 
{
  int cnt;
  byte cmd, codec, ret;
  byte adm;
  byte val;
  char *pch;								// Pointer to character position
	
  if (readLine[0] != '>') {
	Serial.println(F("! ERR: cmd \">\" "));
	return;
  } else { 
	Serial.println(readLine);
	pch = readLine+1;
  }
  if (strlen(readLine) <= 3) {
	Serial.println(F("! ERR: syntax"));
	return;
  }
  pch = strtok(pch, " ,."); cnt = atoi(pch);
  pch = strtok(NULL, " ,."); cmd = atoi(pch);

  if (cnt)
  switch(cmd) {
    case 0:									// admin
	  pch = strtok (NULL, " ,."); adm = atoi(pch);
	  switch (adm) {
		case 0:	// List device Codecs
			Serial.print(F("< "));
			Serial.print(msgCnt);
			Serial.print(F(" 0 0 "));
			Serial.println(codecs,BIN);
			msgCnt++;
		break;
		case 1:	// List codec value for ONE sensor only
			pch = strtok (NULL, " ,."); codec = atoi(pch);
			pch = strtok (NULL, " ,."); val = atoi(pch);
			if (val == 1) onCodec(codec); else offCodec(codec);
			Serial.print("! set Cod = "); Serial.println(codecs,BIN);
		break;
		case 2:	// Ask for statistics
			Serial.print("! Stat = "); Serial.println("");
#if			MEGA==1
			
#endif
		break;
		case 3:	// Debug level
			pch = strtok (NULL, " ,."); debug = atoi(pch);
			if (debug >= 1) { debug = 1; } else { debug = 0; }
			
			Serial.print(F("< "));
			Serial.print(msgCnt);
			Serial.print(F(" 0 3 "));
			Serial.print(debug);
			if (debug) {
				Serial.print(F(" ! Debug ON"));
			}
			Serial.println("");
			msgCnt++;
		break;
		case 4:
			Serial.println(F(VERSION));
		break;
		default:
			Serial.println(F("! ERR admin cmd"));
	  }
	break;
	case 1:								// transmit a key value from the Arduino
		pch = strtok (NULL, " ,."); codec = atoi(pch);
		switch (codec) {
			case KAKU:					// 0
				parseKaku(pch);
			break;
			case ACTION:				// 1
				parseAction(pch);
			break;
			case BLOKKER:				// 2
				Serial.println(F("! ERR: Blokker"));
			break;
			case LIVOLO:				// 5
				parseLivolo(pch);
			break;
#if T_QUHWA==1
			case QUHWA:					// 7
				parseQuhwa(pch);
			break;
#endif
			default:
				Serial.println(F("! ERR: Codec"));
				return;
		}
		msgCnt++;
	break;
	case 2:								// 
		Serial.print(F("! cmd 2 not found "));
	break;
	default:
		Serial.print(F("! Unknown command "));
		Serial.println(cmd);
  }
  msgCnt++;
}

// --------------------------------------------------------------------------------
// Do parsing of Kaku specific command
// print back to caller the received code
// There is a difference for switches and dimmers. Switches do not need dim level 1 (FdP0)
// but the 1 valuw for switches F1. As we cannot make that difference in the 
// Arduino code we also support values "off" and "on" that work for switches as well
// as dimmers (last value).
//
void parseKaku(char *pch)
{
  int group;
  int unit;
  int level;
  KakuTransmitter transmitter(8, 260, 3);
  pch = strtok (NULL, " ,."); group = atoi(pch);
  pch = strtok (NULL, " ,."); unit = atoi(pch);  
  pch = strtok (NULL, " ,."); level = atoi(pch);

  if (pch[0] == 'o') { // pch == 'on' as sent by PI-arduino
	if (pch[1] == 'n') {
		transmitter.sendUnit(group, unit, true);				// "on"
	}
	else transmitter.sendUnit(group, unit, false);				// "off" (must be probably)
  }
  else if (level == 0) {										// value 0 is off for dim and switch
    transmitter.sendUnit(group, unit, false);
  } 
  else if (level >= 1 && level <= 15) {
    transmitter.sendDim(group, unit, level);					// 1 - 15
  } 
  else {
    Serial.println(F("! ERROR dim not between 0 and 15!"));
  }
  Serial.print(F(" ! Kaku send: ")); Serial.println(pch);
}


// --------------------------------------------------------------------------------
// Do parsing of Action specific command
// print back to caller the received code
//
void parseAction(char *pch)
{
	byte group;
	byte unit;
	ActionTransmitter atransmitter(8, 195, 3);		// Timing 195us pulse
	boolean lvl = false;
	pch = strtok (NULL, " ,."); group = atoi(pch);
	pch = strtok (NULL, " ,."); unit = atoi(pch);  
	pch = strtok (NULL, " ,."); if (atoi(pch) == 1) lvl = true;

	if (debug >= 1) {
		Serial.print(F(" ! Action Xmt G:"));
		Serial.print(group);
		Serial.print(F(", U:"));
		Serial.print(unit);
		Serial.print(F(", L:"));
		Serial.println(lvl);
	}
	atransmitter.sendSignal(group, unit, lvl);
}

// --------------------------------------------------------------------------------
// Do parsing of Livolo specific command
// print back to caller the received code
//
// We do expect a level parameter for compatibility with other transmitters
//
void parseLivolo(char *pch)
{
	int group;
	int unit;
	char level;
	Livolo livolo(8);
	pch = strtok (NULL, " ,."); group = atoi(pch);
	pch = strtok (NULL, " ,."); unit = atoi(pch);  
	pch = strtok (NULL, " ,."); level = atoi(pch);
	Serial.print(F("! Livolo:: G: ")); Serial.print(group);
	Serial.print(F(", U: ")); Serial.print(unit);
	Serial.print(F(", L: ")); Serial.println(level);
	livolo.sendButton(group, unit);
}

// --------------------------------------------------------------------------------
// Do parsing of Quhwa specific command
// print back to caller the received code
//
// We do expect a level parameter for compatibility with other transmitters
//
#if T_QUHWA==1
void parseQuhwa(char *pch)
{
	unsigned long group;
	unsigned short unit;
	char level;
	Quhwa quhwa(8);
	pch = strtok (NULL, " ,."); group = atol(pch);
	pch = strtok (NULL, " ,."); unit = atoi(pch);  
	pch = strtok (NULL, " ,."); level = atoi(pch);
	Serial.print(F("! Quhwa:: G: ")); Serial.print(group);
	Serial.print(F(", U: ")); Serial.print(unit);
	Serial.print(F(", L: ")); Serial.println(level);
	quhwa.sendButton(group, unit);
}
#endif

// ************************* RECEIVER STUFF BELOW *********************************

// --------------------------------------------------------------------------------
// WT440
#if S_WT440==1
void showWt440Code(wt440Code receivedCode) {
	
	Serial.print(F("< "));
	Serial.print(msgCnt);
	switch (receivedCode.wconst) {
		case 0x0:
			Serial.print(F(" 3 4 ")); 				// PIR
		break;
		case 0x4:
			Serial.print(F(" 3 3 ")); 				// Battery
		break;
		case 0x6:
			Serial.print(F(" 3 1 "));				// Normal WT440 message format with Humidity
		break;
		case 0x7:
			Serial.print(F(" 3 2 "));				// BMP085 and BMP180 mis-use
		break;
		//default:
	}
	Serial.print(receivedCode.address);				// Values from 0x0 to 0xF (4 bits)
	Serial.print(" ");
	Serial.print(receivedCode.channel);				// Values fro 0x0 to 0x3 (2 bits)
	Serial.print(" ");
	Serial.print(receivedCode.temperature);			// May be used for battery when wconst == 0x4
	Serial.print(" ");
	Serial.print(receivedCode.humidity);			// Can be misused as airpressure is wconst == B111 (7)

	if (debug >= 1) {
		Serial.print(F(" ! WT440:: ")); 
#if A_MEGA==1
		switch (receivedCode.wconst) {
		case 0x00:
			Serial.print(F(" PIR DEV: ")); 				// PIR
			Serial.print(receivedCode.address);
			Serial.print(F(" "));
			Serial.print(receivedCode.temperature);
		break;
		case 0x4:
			Serial.print(F(" BATT DEV: ")); 			// Battery
			Serial.print(receivedCode.address);
			Serial.print(F(" "));
			Serial.print((float)receivedCode.temperature / 10, 1);
			Serial.print(F("%"));
		break;
		case 0x6:
			Serial.print(F(" WT440 DEV: "));			// Normal WT440 message format with Humidity
			Serial.print(receivedCode.address);
			Serial.print(F(" TEMP: "));	
			Serial.print((float)(receivedCode.temperature - 6400) / 128, 1);
			Serial.print(F(" HUM: "));
			Serial.print(receivedCode.humidity,1);
			Serial.print(F("%"));
		break;
		case 0x7:
			Serial.print(F(" BMP DEV: "));				// BMP085 and BMP180 mis-use
			Serial.print(receivedCode.address);
			Serial.print(F(" TEMP: "));
			Serial.print((float)(receivedCode.temperature - 6400) / 128, 1);
			Serial.print(F(" P: "));
			Serial.print(receivedCode.humidity + 930);
		break;
		//default:
		}
#endif

# if STATISTICS==1
		Serial.print(F(", W: "));Serial.print(receivedCode.wconst);
		Serial.print(F(", P: ")); Serial.print(receivedCode.par);
		Serial.print(F(", P1: ")); Serial.print(receivedCode.min1Period);
		Serial.print(F("-")); Serial.print(receivedCode.max1Period);
		Serial.print(F(", P2: ")); Serial.print(receivedCode.min2Period);
		Serial.print(F("-")); Serial.print(receivedCode.max2Period);
# endif
	}
	Serial.println("");
	msgCnt++;
}
#endif

// --------------------------------------------------------------------------------
// AURIOL
// As the preprocessor is gettig wild when S_AURIOL != 1, we for the moment
// have to comment the code out. As we use code 0-7 for the WT440 messages,
// Auriol starts at 8
#if S_AURIOL==1
//
void showAuriolCode(auriolCode receivedCode) {
	
	Serial.print(F("< "));
	Serial.print(msgCnt);
	Serial.print(F(" 3 8 "));
	Serial.print(receivedCode.address);
	Serial.print(F(" "));
	Serial.print(receivedCode.channel);
	Serial.print(F(" "));
	Serial.print(receivedCode.temperature);
	Serial.print(F(" "));
	Serial.print(receivedCode.humidity);

	if (debug >= 1) {
		Serial.print(F(" ! Auriol Cs: ")); Serial.print(receivedCode.csum);
		Serial.print(F(", N1: ")); Serial.print(receivedCode.n1,BIN);
		Serial.print(F(", N2: ")); Serial.print(receivedCode.n2,BIN);	
# if STATISTICS==1
		Serial.print(F(", 1P: ")); Serial.print(receivedCode.min1Period);
		Serial.print(F("-")); Serial.print(receivedCode.max1Period);
		Serial.print(F(", 2P: ")); Serial.print(receivedCode.min2Period);
		Serial.print(F("-")); Serial.print(receivedCode.max2Period);
# endif
	}
	Serial.println("");
	msgCnt++;
}
#endif

// --------------------------------------------------------------------------------
// KAKU
// Regular klikaanklikuit devices
// Callback function is called only when a valid code is received.
//
void showKakuCode(KakuCode receivedCode) {

  // Print the received code. 2 for received codes and 0 for codec of Kaku
  Serial.print(F("< "));
  Serial.print(msgCnt);
  Serial.print(F(" 2 0 "));
  Serial.print(receivedCode.address);
  msgCnt++;
  
  if (receivedCode.groupBit) {
    Serial.print(F(" G "));
  } 
  else {
    Serial.print(F(" "));
    Serial.print(receivedCode.unit);
  }
  
  switch (receivedCode.switchType) {
    case KakuCode::off:
      Serial.print(F(" 0"));
      break;
    case KakuCode::on:
      Serial.print(F(" on"));
      break;
    case KakuCode::dim:
      // Serial.print(F(" dim "));
	  if (receivedCode.dimLevelPresent) {
		Serial.print(F(" "));
		Serial.print(receivedCode.dimLevel);
	  }
    break;
  }
  if (debug) {
	Serial.print(F(" ! Kaku G: "));
	Serial.print(receivedCode.address);
	Serial.print(F(" N "));
	Serial.print(receivedCode.unit);
	Serial.print(F(" "));
	switch (receivedCode.switchType) {
		case KakuCode::off:
			Serial.print(F(" 0"));
		break;
		case KakuCode::on:
			Serial.print(F(" on"));
		break;
		case KakuCode::dim:
			Serial.print(F(" dim "));
			if (receivedCode.dimLevelPresent) {
				Serial.print(F(" "));
				Serial.print(receivedCode.dimLevel);
			}
		break;
	}
  }
  Serial.println("");
  Serial.flush();
}



// --------------------------------------------------------------------------------
// LIVOLO
//
void showLivoloCode(livoloCode receivedCode) {
	
	Serial.print(F("< "));
	Serial.print(msgCnt);
	Serial.print(F(" 2 5 "));
	Serial.print(receivedCode.address);
	Serial.print(F(" "));
	Serial.print(receivedCode.unit);
	Serial.print(F(" "));
	Serial.print(receivedCode.level);

	if (debug >= 1) {
		Serial.print(F(" ! Livolo ")); 
#if STATISTICS==1
		Serial.print(F(", P1: ")); Serial.print(receivedCode.min1Period);
		Serial.print(F("-")); Serial.print(receivedCode.max1Period);
		Serial.print(F(", P3: ")); Serial.print(receivedCode.min3Period);
		Serial.print(F("-")); Serial.print(receivedCode.max3Period);
#endif
	}
	Serial.println("");
	msgCnt++;
}

// --------------------------------------------------------------------------------
// KOPOU switches
//
#if R_KOPOU==1
void showKopouCode(kopouCode receivedCode) {
	Serial.print(F("< "));
	Serial.print(msgCnt);
	Serial.print(F(" 2 6 "));
	Serial.print(receivedCode.address);
	Serial.print(F(" "));
	Serial.print(receivedCode.unit);
	Serial.print(F(" "));
	Serial.print(receivedCode.level);
	
	if (debug >= 1) {
		Serial.print(F(" ! Kopou ")); 
#if STATISTICS==1
		Serial.print(F(", P: ")); Serial.print(receivedCode.period);
		Serial.print(F(", P1: ")); Serial.print(receivedCode.minPeriod);
		Serial.print(F("-")); Serial.print(receivedCode.maxPeriod);
#endif
	}
	Serial.println("");
	msgCnt++;
}
#endif

// --------------------------------------------------------------------------------
// QUHWA doorbell protocol
//
#if R_QUHWA==1
void showQuhwaCode(quhwaCode receivedCode) {
	Serial.print(F("< "));
	Serial.print(msgCnt);
	Serial.print(F(" 2 7 "));
	Serial.print(receivedCode.address);
	Serial.print(F(" "));
	Serial.print(receivedCode.unit);
	Serial.print(F(" "));
	Serial.print(receivedCode.level);
	
	if (debug >= 1) {
		Serial.print(F(" ! Quhwa ")); 
# if STATISTICS==1
		Serial.print(F(", 1P: ")); Serial.print(receivedCode.min1Period);
		Serial.print(F("-")); Serial.print(receivedCode.max1Period);
		Serial.print(F(", 3P: ")); Serial.print(receivedCode.min3Period);
		Serial.print(F("-")); Serial.print(receivedCode.max3Period);
# endif
	}
	Serial.println("");
	msgCnt++;
}
#endif


// --------------------------------------------------------------------------------
// REMOTE
// General receiver for old Kaku, Impulse, etc.
// receivedCode is an unsigned gong with address and unit info
// At this moment the code is tuned for Action remotes.
// Must be recognizing the correct remote later (based on period?)
//
void showRemoteCode(unsigned long receivedCode, unsigned int period) {
	int i;
	byte level = 0;
	byte unit = 0;
	short address = 0;
	byte codec = 1;

	// Unfortunately code is Base3
	unsigned long code = receivedCode;

	// Action codec
	if ( (period > 120 ) && (period < 200 ) ) {
		for (i=0; i<2; i++) {
			level = level * 10;
			level += code % 3;
			code = code / 3;
		}
		// two bits, either 02 or 20, the 0 determines the value
		if (level == 20) { level = 0; }
		else { level = 1; }
		// 5 bits, 5 units. The position of the 0 determines the unit (0 to 5)	
		for (i =4; i >= 0; i--) {
			if ((code % 3) == 0) unit= i;
			code =  code / 3;
		}
		// Position of 1 bit determines the values
		for (i=0; i < 5; i++) {
			if ((code % 3) == 1) address += ( B00001<<i );
			code =  code / 3;
		}
	}
	// Following code should be equal for ALL receivers of this type
	Serial.print(F("< "));
	Serial.print(msgCnt);
	Serial.print(F(" 2 "));					
	Serial.print(codec);		// action is type 1, blokker type 2, old=3, elro=4, livolo 5.
	Serial.print(F(" "));
	Serial.print(address);
	Serial.print(F(" "));
	Serial.print(unit);
	Serial.print(F(" "));
	Serial.print(level);

	if (debug >= 1) {
		Serial.print(F(" ! Remote:: period: ")); 
		Serial.print(period); 
#if STATISTICS==1
		Serial.print(F(", P: ")); Serial.print(period);
#endif	 
	}
	Serial.println("");
	msgCnt++;
}

