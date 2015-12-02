/* --------------------------------------------------------------------------------
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
* Connect the receiver to digital pin 4, sender to pin 5. Other definitions in LamPI.h
* NOTE: You can enable/disable modules in the LamPI.h file
*		Sorry for making the code in this file so messy :-) with #if
* ---------------------------------------------------------------------------------
*/

/*
  This file implements the ESP8266 side of the LamPI Gateway.
  setup:
  1. The program connects to a Wifi access point (Parameters specified in ESP-gateway.ino)
  2. The program then connects to a server and opens the socket connection (port 5002).
  3. Depending on the settings in the .h file sensors are initialized and receiver handlers
	with their interrupt routines are attached to the interrupt pin.
  loop:
  1. Manage connection, if lost reestablish
  2. In the main loop() we read the socket for incoming (READ) commands from the server daemon,
	and after decodign we send messages over the air (433MHz) to devices or sensors
  3. After an elapsed time report the values of sensors that are enabled (in .h file)
  interrupt:
  1. If messages are recognized on one of the emables receivers, the corresponding handler function
	is called (defined in setup), and values are reported back to daemon over the socket.

  So: baudrate and supported potocolmust be set corecly in the ArduinoGateway.h file
  And these must match the lampi_arduino.h definitions in file on ~/receivers/arduino on Raspberry
*/

// Please look at http://platenspeler.github.io/HardwareGuide/Arduino-Gateway/gatewayMsgFormat.html
// for definitions of the message format for the Arduino Gateway.
// Command "> 111 0 4" will output the version of the Gateway software
#define VERSION " ! V. 1.7.5, 151125"

#include "ESP-Gateway.h"

// General Include
//
#include "LamPI_ESP.h"							// PIN Definitions
#include <ESP8266WiFi.h>
#include <wifiQueue.h>
#include <ESP.h>
#include <Base64.h>

#include <Wire.h>
#include <Time.h>
#include "OneWireESP.h"
#include <Streaming.h>          //http://arduiniana.org/libraries/streaming/
#include <ArduinoJson.h>
#include <pins_arduino.h>

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
// that are defined inside an #if (for unknown reasons, complex parameters in functions 
// will then not be recognized)
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

// Use WiFiClient class to create TCP connections
// For the gateway we will keep the connection open as long as we can
// and re-estanblish when broken.
WiFiClient client;
uint8_t retries = 0;

//
// Sensors Include
//
#if S_DALLAS==1
#include <OneWireESP.h>
#include <DallasTemperature.h>
OneWire oneWire(A_ONE_WIRE);	// pin  is set in LAMPI_ESP.h 
DallasTemperature sensors(&oneWire);
int numberOfDevices; 			// Number of temperature devices found
#endif

#if S_HTU21D==1
#include <HTU21D.h>
HTU21D myHumidity;				// Init Sensor(s)
#endif

#if S_BMP085==1
#include <bmp085.h>
BMP085 bmp085;
#endif

#if S_BH1750==1
#include <BH1750FVI.h>
BH1750FVI LightSensor;
#endif

#if S_DS3231==1
// #include <Time.h>			//http://playground.arduino.cc/Code/Time
#include "DS3232RTC.h"			//http://github.com/JChristensen/DS3232RTC
#endif

unsigned long myTime;			// fill up with millis();
boolean debug;					// If set, more informtion is output to the serial port
unsigned int msgCnt=1;			// Not unique, as at some time number will wrap.
boolean wifiReConnect;
int sensorLoops;
char tbuf[256];					// transmit buffer

// Use a bit array (coded in long) to keep track of what protocol is enabled.
// Best is to make this dynamic and not compile time. However,only the Arduino Mega has enough memory
// to run all codecs with all their declarations without problems.
//
unsigned long codecs;			// must be at least 32 bits for 32 positions. Use long instead of array.
QueueChain wQueue;			// Wifi Queue


// --------------------------------------------------------------------------------
// SETUP
// Setup all sensor routines and the 433 Interrupts
//
void setup() {
	// Create a transmitter using digital pin A_TRANSMITTER (default is GPIO14) to transmit,
	// with a period duration of 260ms (default), repeating the transmitted
	// code 2^3=8 times.
	pinMode(A_TRANSMITTER, OUTPUT);
	digitalWrite(A_TRANSMITTER, LOW);
	
	Serial.begin(BAUDRATE);			// As fast as possible for USB bus, defined in Transeiver433.h
	wdt_enable(255);				// Watchdog time reset 200 ms, hope it works
	
	wifiReConnect=false;
	msgCnt = 0;
	debug = DEBUG;					// Define in .h file
	sensorLoops=0;
	codecs = 0;
	Serial << F("! debug: ") << debug << endl;

	// Setup WiFi client connection
	if (WifiConnect( (char *) _SSID, (char *)_PASS) < 0) {
		Serial << F("Error Wifi network SSID: ") << _SSID << ":" << _PASS << endl;
	}
	if (HostConnect( (char *) _HOST, _PORT) < 0) {
		Serial << F("Error Host Connection ") << _HOST << ":" << _PORT << endl;
	}
	// This is one of the most important parameters of the sketch. 100 works almost OK,
	// but it should be a low as powwible to avoid the client.available() call to block
	// interrupts etc.
	// Getting and handling a message of around 120 characters on reasonable speed
	// should not take more time than this.
	client.setTimeout(10);							// XXX set below 50 msec, and as low as possible

#if S_DS3231==1
	setSyncProvider(RTC.get);
	Serial << F("! RTC Sync");
    if (timeStatus() != timeSet) Serial << F(" FAIL!"); else Serial << F(" OK");
    Serial << endl;
	onCodec(ONBOARD);
#endif
  
	// Initialize receiver on interrupt 0 (= digital pin 2), calls the callback (for example "showKakuCode")
	// after 2 identical codes have been received in a row. (thus, keep the button pressed for a moment)
#if R_KAKU==1
	KakuReceiver::init(-1, 2, showKakuCode);
#endif
#if R_ACTION==1
	RemoteReceiver::init(-1, 2, showRemoteCode);
#endif
#if R_LIVOLO==1
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

	// Change interrupt mode to CHANGE (on flanks)
	InterruptChain::setMode(digitalPinToInterrupt(A_RECEIVER), CHANGE);
  
	// Define the interrupt chain
	// The sequence might be relevant, defines the order of execution, put easy protocols first
	// onCodec: First 16 bits are for sensors (+16), last 16 bits for devices,
	// By removing a line below, that type of device will not be scanned anymore :-)
	//
#if S_WT440==1
	InterruptChain::addInterruptCallback(digitalPinToInterrupt(A_RECEIVER), wt440Receiver::interruptHandler);  
	onCodec(WT440);
#endif
#if S_AURIOL==1
	InterruptChain::addInterruptCallback(digitalPinToInterrupt(A_RECEIVER), auriolReceiver::interruptHandler);
	onCodec(AURIOL);
#endif
#if R_ACTION==1
	InterruptChain::addInterruptCallback(digitalPinToInterrupt(A_RECEIVER), RemoteReceiver::interruptHandler);
	onCodec(ACTION);
#endif
#if R_KAKU==1
	InterruptChain::addInterruptCallback(digitalPinToInterrupt(A_RECEIVER), KakuReceiver::interruptHandler);
	onCodec(KAKU);
#endif
#if R_LIVOLO==1
	InterruptChain::addInterruptCallback(digitalPinToInterrupt(A_RECEIVER), livoloReceiver::interruptHandler);
	onCodec(LIVOLO);
#endif
#if R_KOPOU==1
	InterruptChain::addInterruptCallback(digitalPinToInterrupt(A_RECEIVER), kopouReceiver::interruptHandler);	
	onCodec(KOPOU);
#endif
#if R_QUHWA==1
	InterruptChain::addInterruptCallback(digitalPinToInterrupt(A_RECEIVER), quhwaReceiver::interruptHandler);	
	onCodec(QUHWA);
#endif


// Sensors
#if S_DALLAS==1
	sensors.begin();
	numberOfDevices = sensors.getDeviceCount();
	Serial.print("! #Dallas: "); Serial.println(numberOfDevices);
	if (numberOfDevices > 0) { onCodec(ONBOARD); onCodec(C_DALLAS); }
#endif
#if S_HTU21D==1
	myHumidity.begin(); 
	myHumidity.setResolution(0);
	if (myHumidity.readHumidity() == 998) {
		Serial.println(F("! No HTU21D")); 				// First read value after starting does not make sense.  
	}
	else { onCodec(ONBOARD); onCodec(C_HTU21D); );
#endif
#if S_BMP085==1
	bmp085.begin(); 
	if (bmp085.Calibration() == 998) { Serial.println(F("! No BMP085")); }	// OnBoard
	else { onCodec(ONBOARD); onCodec(C_BMP085); };
#endif
#if S_BH1750==1
	LightSensor.begin();
	onCodec(ONBOARD);
	LightSensor.SetAddress(Device_Address_H);				//Address 0x23 or 0x5C
	LightSensor.SetMode(Continuous_H_resolution_Mode2);
	if (LightSensor.GetLightIntensity() == (int) -1) {
		Serial.println(F("! No BH1750"));
	}
	else { onCodec(ONBOARD); onCodec(C_BH1750); }
#endif

	printCodecs();
	if (debug==1) {
		Serial << F("! Pin A_RECEIVER is pin ") << A_RECEIVER << F(" is interrupt: ") ;
				Serial << digitalPinToInterrupt(A_RECEIVER) << endl;
		Serial << F("! Pin A_TRANSMITTER is pin ") << A_TRANSMITTER << endl;
		
	}
	myTime = millis();
}

// --------------------------------------------------------------------------------
// MAIN LOOP 
// As we have limited possibilities for async handling of Serial and Wifi we will
// use .available and loop constantly
// We insert delay() calls in this code as it blocks the user-level WiFi stuff
// so that our receiver interrupts will be handled as well.
// Importants is that we do not loose incoming Wifi messages so we have to call client.availble
// on a regular basis, on the other side our 433receiver must not miss too much traffic and
// be able to send it to the server (using the same WiFi routines).
// As we keep the loop() short the WiFi background processes will also run on a regular basis.

void loop() {
  digitalWrite(A_TRANSMITTER, LOW);					// make sure digital transmitter pin is low when not used

  // WiFI has priority over Serial commands (probably depreciated in next release).
  //
  if (wifiReConnect){
	Serial << F("loop:: Should re-connect") << endl;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
	Serial << F("Network reconnect ") << retries << endl;
	if (retries == 0) {								// START WLAN connect. Do only once and wait for answer
		WiFi.begin(_SSID, _PASS);
		// wdt_reset();
		retries =1;
	}
	else {
		delay(500);
		Serial << "." ;
		if (retries == 50) {
			Serial << endl;
			retries = 1;
		}
		retries++;
	}	
	return;											// Short Loop
  }
  else {
	retries = 0;									// If WLAN connected reset retries
  }

  if (!client.connected()) {						// If HOST connection is lost
	Serial << F("! Reconnecting to host ...") << _HOST << endl;
	if (HostReconnect((char *)_HOST, _PORT) < 0){ 
		// Reconnect not yet successful
		delay(1000);
	}
	else {
		Serial << F("! Client re-connected") << endl;
		wifiReConnect=false;
	}
	return;
  }

  delay(3);
  WifiReceive();									// Read the Wifi buffers

  delay(3);
  if (debug > 1) QueueChain::printQueue();

  handleQueue();  // Read if interrupts put something in QUEUE
  
  // Handle Sensors reading (based on time)
  // Lowest priority, lowest in chain. As long as we read these messages once in a while we're 
  // OK.
  readSensors();									// Better for callbacks if there are sleeps
}


// ***************************** Codecs Admin *************************************
// 

// --------------------------------------------------------------------------------
// Switch a codec to on.
//
void onCodec (byte codec) {
	codecs |= ( (unsigned long) 0x0001 << codec	);		// AND

  // We support dynamic enablement and disable of sensors and devices
}

// --------------------------------------------------------------------------------
// We really should disable interrupts for codecs that are in the interrupt chain too.
//
void offCodec (byte codec) {
	codecs &= ~( (unsigned long) 0x0001 << codec );		// NAND, no effect for 0, but for 1 bit makes 0

	// We support dynamic enablement and disable of sensors and devices
}

// --------------------------------------------------------------------------------
//
//
void setCodec (byte codec, byte val) {
  if (val = 0) offCodec(codec);
  else onCodec(codec);
}

// --------------------------------------------------------------------------------
// IF CODEC
// Returns true when the codec is set and used.
//
boolean ifCodec(byte codec) {
		if ((codecs >> codec) & 0x0001) return(true);
		else return(false);
}


// --------------------------------------------------------------------------------
//
//
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
	
	if ((codecs >> C_DALLAS) & 0x0001) Serial.print(F("C_DALLAS "));
	if ((codecs >> C_HTU21D) & 0x0001) Serial.print(F("C_HTU21D "));
	if ((codecs >> C_BMP085) & 0x0001) Serial.print(F("C_BMP085 "));
	if ((codecs >> C_BH1750) & 0x0001) Serial.print(F("C_BH1750 "));
	Serial.println();
}


// ************************* SENSORS PART *****************************************


// --------------------------------------------------------------------------------
// Read ONBOARD sensors
//
void readSensors() {

	// Only start reading sensors when enough time hase elapsed.
	if ((millis() - myTime) > 63000) {				// 63 seconds, so avoiding collisions with other cron jobs
	  myTime = millis();
	  // We use sensorLoops to record how many times we came here before timer expired
	  Serial << F("Sensor Loops: ") << sensorLoops << F(", Avg: ") << (63000/sensorLoops) << F(" ms") << endl;
	  sensorLoops = 0;
		
#if S_DALLAS
		uint8_t ind;
		DeviceAddress tempDeviceAddress; 			// We'll use this variable to store a found device address
		sensors.requestTemperatures();
		for(int i=0; i<numberOfDevices; i++)
		{
			// Search the wire for address
			if(sensors.getAddress(tempDeviceAddress, i))
			{
				float tempC = sensors.getTempC(tempDeviceAddress);
				// Output the device ID
				if (debug) {
					Serial.print("! DS18B20 dev ");
					Serial.print(i, DEC);
				}
				Serial.println();
				delay(10);
				if (SensorTransmit(_ADDR, (2+i), (char *)"esp8266" ,(char *) "temperature", tempC) < 0) {
					Serial.println(F("ERROR SensorTransmit"));
				}

			} 
			//else ghost device! Check your power requirements and cabling
		}
#endif

#if S_HTU21D==1
		// HTU21 or SHT21
		float humd = myHumidity.readHumidity();
		float temp = myHumidity.readTemperature();
		if (((int)humd != 999) && ((int)humd != 998 )) {	// Timeout (no sensor) or CRC error
			if (debug) {
				Serial.print(F(" ! HTU21 T: "));
				Serial.print(temp/10);
			}

			if (SensorTransmit(_ADDR, 0, (char *)"esp8266" ,(char *) "temperature", tempC) < 0) {
				Serial.println(F("ERROR SensorTransmit"));
			}
			if (SensorTransmit(_ADDR, 0, (char *)"esp8266" ,(char *) "humidity", tempC) < 0) {
				Serial.println(F("ERROR SensorTransmit"));
			}
			Serial.println();
			msgCnt++;
		}
		else if (debug>=1) { Serial.println(F(" ! No HTU21")) };
#endif

#if S_BMP085==1		
		// BMP085 or BMP180
	  if (bmp085.Calibration() != 998) {					// Hope we can call this function without exception
		short temperature = bmp085.GetTemperature();
		if ((temperature != 998) && (temperature != 0)){
			long pressure = bmp085.GetPressure();
			//float altitude = (float)44330 * (1 - pow(((float) pressure/bmp085.p0), 0.190295));
			if (debug>=1) {
				Serial << F("! BMP: t: ") << (float)(temperature/10) << endl;
				delay(30);							// Too make sure that this message gets printed
			}
			if (SensorTransmit(_ADDR, 1, (char *)"esp8266" ,(char *)"temperature", (float)temperature/10) < 0) {
				Serial.println(F("ERROR SensorTransmit"));
			}
			delay(10);
			if (SensorTransmit(_ADDR, 1, (char *)"esp8266", (char *)"airpressure", (pressure/100)) < 0) {
				Serial.println(F("ERROR SensorTransmit"));
			}
			Serial.println();
			msgCnt++;
		}
		else { Serial.println(F(" ! Error reading BMP085")); }
	  }
	  else { if (debug>=1) Serial.println(F(" ! No BMP085")); }
#endif

#if S_BH1750==1
		uint16_t lux = LightSensor.GetLightIntensity();			// Get Lux value
		if (lux != (int) -1) {

			float t = temperature / 10;
			if (SensorTransmit(_ADDR, 1, (char *)"esp8266", (char *)"luminescense", (float) lux) < 0) {
				Serial.println(F("ERROR SensorTransmit"));
			}
			Serial.println();
			msgCnt++;
		}
#endif
	}
	else { sensorLoops++; }
}


// ************************ 433 TRANSMITTER PART **************************************


// ------------------------------------------------------------------------------------
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
	Quhwa quhwa(A_TRANSMITTER);
	pch = strtok (NULL, " ,."); group = atol(pch);
	pch = strtok (NULL, " ,."); unit = atoi(pch);  
	pch = strtok (NULL, " ,."); level = atoi(pch);
	Serial.print(F("! Quhwa:: G: ")); Serial.print(group);
	Serial.print(F(", U: ")); Serial.print(unit);
	Serial.print(F(", L: ")); Serial.println(level);
	quhwa.sendButton(group, unit);
}
#endif

// ********************** WIFI SECTiON ********************************************
// This section contains the Wifi functions that we use for the ESP gateway.
//
// NOTE: We cannot use blocking functions or the ESP watchdog will reset
//	the chip and we get exceptions. So make sure to return to loop as soon as possible
// and in next loopt we'll reconnect again if necessary.
//
// NOTE2: Make sure that interrupt routines return as soon as possibe too.
//	So these service routines must NOT reset connections or so as it takes too much time.


// --------------------------------------------------------------------------------
// Function to join the Wifi Network
// XXX Maybe we should make the reconnect shorter in order to avoid watchdog resets.
//	It is a matter of returning to the loop() asap and make sur ein next loop
//	the reconnect is done first thing.
//
int WifiConnect(char * ssid, char * password){

	// We start by connecting to a WiFi network 
	Serial.print("! Connecting to: "); Serial.println(ssid);
	int agains = 0;
	
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
		agains++;
		if (agains == 50) {
			agains = 0;
			Serial.println();
		}
	}
	Serial.print("! WiFi connected. IP address: "); Serial.println(WiFi.localIP());
	return(0);
}

// --------------------------------------------------------------------------------
// Function to establish a Wifi connection to host
//
int HostConnect(char * host, int httpPort) {

	Serial.print(F("! connecting to "));
	Serial.println(host);
	
	if (!client.connect(host, httpPort)) {
		Serial.println(F("! ERROR connection failed"));
		return (-1);
	}
	return(0);
}

// --------------------------------------------------------------------------------
//
// Loop until connected again
//
int HostReconnect(char *host, int httpPort){

	wifiReConnect=true;									// Make sure we do not re-enter this routine
	if (WiFi.status() != WL_CONNECTED) {				// If not connected to a network
		Serial << F("! ERROR: Not connected to WLAN ...") << endl;
		return(-1);
	}
	
	if (!client.connected()) {
		delay(10);
		if (!client.connect(host, httpPort)) {
			Serial << F("! ERROR reconnect ") << host << F(":") << httpPort << endl;
			return -1;
		}
		return(0);
	}
	else {
		wifiReConnect=false;
		Serial << F("! Reconnected to host") << endl;
	}
	return(0);
}


// --------------------------------------------------------------------------------
// Function transmitting Sensor Wifi messages to host
//
int SensorTransmit( uint32_t address, uint8_t channel, char *brand, char *label, float value) {

	int ival = (int) value;					// Make interger part
	int fval = (int) ((value - ival)*10);	// Fraction

	sprintf (tbuf,
	"{\"tcnt\":\"%d\",\"type\":\"json\",\"action\":\"sensor\",\"brand\":\"%s\",\"address\":\"%lu\",\"channel\":\"%d\",\"%s\":\"%d.%d\"}"
	, msgCnt, brand, address, channel, label, ival, fval);

	// We cannot block the program in interrupt whith reconnections.
	// Therefore only is connected we're able to send the message.
	// Just hope reconnected will not happen too often.
	
	if (client.connected()) {
		client.write((const char *)tbuf, strlen(tbuf));
		Serial << F("SEND: ") << address << ":" << channel << " <" << strlen(tbuf) << "> " << tbuf << endl;
		msgCnt++;
	}
	else {
		Serial << F("SensorTransmit:: Not connected") << endl;
		delay(10);
		// XXX This is tricky! But is the received loop() does not get time we have to/
		wdt_reset();
		wifiReConnect=true;
	}
	return(0);
}

// ---------------------------------------------------------------------
// SENSOR QUEUE
//
int sensorQueue(queueItem qi) {
	char tbuf[128];
	int ival = (int) qi.value;					// Make interger part
	int fval = (int) ((qi.value - ival)*10);	// Fraction
	// Copy to string
	sprintf (tbuf,
		"{\"tcnt\":\"%d\",\"type\":\"raw\",\"action\":\"sensor\",\"brand\":\"%s\",\"address\":\"%lu\",\"channel\":\"%d\",\"%s\":\"%d.%d\"}"
		, msgCnt, qi.brand, qi.address, qi.channel, qi.label, ival, fval);
	// Send to WiFi Transmit
	if (client.connected()) {
		client.write((const char *)tbuf, strlen(tbuf));
		Serial << F("SEND: ") << qi.address << ":" << qi.channel << " <" << strlen(tbuf) << "> " << tbuf << endl;
		msgCnt++;
	}
	else {
		return(-1);
	}
	// Enable resources
	return(0);
}

// ---------------------------------------------------------------------
// DEVICE QUEUE
// Handle the queue with your devices glasses on ...
//
int deviceQueue(queueItem qi) {
	char tbuf[128];
	char message[16];
	
	// Copy to string
	sprintf (tbuf,
		"{\"tcnt\":\"%d\",\"type\":\"raw\",\"action\":\"gui\",\"cmd\":\"%s\",\"gaddr\":\"%lu\",\"uaddr\":\"%d\",\"val\":\"%s\",\"message\":\"%s\"}"
		, msgCnt, qi.cmd, qi.gaddr, qi.uaddr, qi.val, qi.message);
	// Send to WiFi Transmit
	if (client.connected()) {
		client.write((const char *)tbuf, strlen(tbuf));
		Serial << F("SEND: ") << qi.gaddr << ":" << qi.uaddr << " <" << strlen(tbuf) << "> " << tbuf << endl;
		msgCnt++;
	}
	else {
		return(-1);
	}
	return(0);
}

// ---------------------------------------------------------------------
// HANDLE QUEUE
// In order to keep the interrupt secin as short as possible (knowing Wifi calls
// in general take a lot of time) we make a queue for transmissions that result from 
// incoming interrupts of the 433mHz receiver
// 
// This function calls the appripriate handling function for either sensors or devices
// sensors: {"tcnt":msgCnt,"type"="json","action":"sensor","brand":"wt440","address":address,"channel":channel,label:value}
//		where label can be "temperature","humidity","airpressure"
//		and value is the corresponding float value.
// 	and example
// devices: {"tcnt":msgCnt,"type":"raw","action":"gui","cmd":"zwave","gaddr":"868","uaddr":"7","val":"off","message":"!R7D7F0"}
//
int handleQueue() {
	queueItem qi;
	char tbuf[128];
	while ((QueueChain::processQueue(&qi)) >= 0) {
		// We have a valid action the queue
		// Maybe make the action an enumerated type ...
		if (strcmp(qi.action, "sensor")==0) {
			// sensor
			sensorQueue(qi);
		}
		else if (strcmp(qi.action, "gui")==0) {
			// Could be gui 
			Serial << F("handleQueue:: Gui read") << endl;
			deviceQueue(qi);
		}
		else if (strcmp(qi.action, "handset")==0) {
			// or handset
			Serial << F("handleQueue:: Handset read") << endl;
		}
		else {
			Serial << F("handleQueue:: ERROR unknow action") << endl;
		}
	}
	return(0);
}


// --------------------------------------------------------------------------------
// Wifi Receive
//
int WifiReceive() {
	char jline[300];
	int ind;

	// Read all the lines of the reply from server and print them to Serial
	//
	if (!client.connected()) 	{
		Serial << F("! WifiReceive: not connected") << endl;
		return(-1);
	}
	else {
		delay(10);
	}
	
	// Reading until a \r\n does not work and leads to timeout.
	// Server will only send JSON messages which are packed in {}
	if (client.available()) {
		// Read until } character, but as not included as part of result, append immediately
		String line = client.readStringUntil('}') + "}"; 
		if (debug) {
			Serial << F("READ  <") << line.length() << "> " << line ;
		}

		// Now decode the JSON string received from the server
		StaticJsonBuffer<256> jsonBuffer;
		JsonObject& root = jsonBuffer.parseObject(line.c_str());
		
		if (!root.success()) {
			Serial  << F(" ! ERROR WifiReceive:: Json Decode");
			return(-1);
		}
		int tcnt = root["tcnt"];
		const char * type  = root["type"];
		const char * action = root["action"];
		const char * cmd = root["cmd"];
		unsigned int gaddr = root["gaddr"];
		byte uaddr = root["uaddr"];
		const char * val = root["val"];
		//const char * message = root["message"];
		
		// Parse and transmit KAKU message	
		if (strcmp(cmd, "kaku")==0) {
			int value = atoi(val) /2 ;						// XXX Dimlevel of devices is 0-15, LamPI 0-31
			KakuTransmitter transmitter(A_TRANSMITTER, 260, 3);
			if (val[0] == 'o') {
				if (val[1]=='n') transmitter.sendUnit(gaddr, uaddr, true);
				else if (val[1]=='f') transmitter.sendUnit(gaddr, uaddr, false);
				else Serial << F(" ! wifiReceive:: Unknown button command");
			}
			else if (value == 0) {
				transmitter.sendUnit(gaddr, uaddr, false);
			} 
			else if (value >= 1 && value <= 15) {
				transmitter.sendDim(gaddr, uaddr, value);
			} 
			else {
				Serial << F(" ! ERROR dim not between 0 and 15!");
			}
		} 
		// This is an action command
		else if (strcmp(cmd, "action")==0) {
			Serial << F(" ! action cmd");
			ActionTransmitter atransmitter(A_TRANSMITTER, 195, 3);		// Timing 195us pulse
			if (strcmp(val, "on")==0) atransmitter.sendSignal(gaddr,uaddr,true);
			else atransmitter.sendSignal(gaddr,uaddr,false);
		}
		// Livolo
		else if (strcmp(cmd, "livolo")==0) {
			Serial << F(" ! livolo cmd");
			Livolo livolo(A_TRANSMITTER);
			livolo.sendButton(gaddr, uaddr);
		}
		// zwave
		else if (strcmp(cmd, "zwave")==0) {
			Serial << F(" ! Zwave cmd Ignored");
		} 
		else {
			Serial << F(" ! Command not recognized: ") << cmd;
		}
		
		Serial << endl;									// Print nedline
	}
//XXX	InterruptChain::enable(digitalPinToInterrupt(A_RECEIVER));			// XXX Set interrupts on
	return(0);
}


// *********************** 433 RECEIVER STUFF BELOW *******************************
// This is the section where all the specific 433MHz handling is done.
// It contains all the callback routines used in the interrupts.

// --------------------------------------------------------------------------------
// WT440
#if S_WT440==1
void showWt440Code(wt440Code receivedCode) {
	// 
	queueItem item;
	item.address = receivedCode.address;
	item.channel = receivedCode.channel;
	item.value = (float)receivedCode.temperature/10;
	sprintf(item.brand,"wt440");
	sprintf(item.action,"sensor");
	sprintf(item.type,"json");
	
	float temperature = ((float)(receivedCode.temperature - 6400)) / 128;
	switch (receivedCode.wconst) {
		case 0x0:
			item.value = (float)(receivedCode.temperature);
			sprintf(item.label,"pir");
			QueueChain::addQueue(item, NULL);
			break;
		case 0x4:
			// item.value = (float)(receivedCode.temperature/10;
			sprintf(item.label,"battery");
			QueueChain::addQueue(item, NULL);
			break;
		case 0x6:
			item.value = ((float)(receivedCode.temperature - 6400)) / 128;
			sprintf(item.label,"temperature");
			QueueChain::addQueue(item, NULL);
			
			item.value = (float)(receivedCode.humidity);
			sprintf(item.label,"humidity");
			QueueChain::addQueue(item, NULL);
			break;
		case 0x7:
			item.value = ((float)(receivedCode.temperature - 6400)) / 128;
			sprintf(item.label,"temperature");
			QueueChain::addQueue(item, NULL);
			
			item.value = (float)(receivedCode.humidity);
			sprintf(item.label,"airpressure");
			QueueChain::addQueue(item, NULL);
			break;
		default:
			Serial << F("showWT440Code:: Unknown opcode: ") << receivedCode.wconst << endl;
	}
#if STATISTICS==1
	if (debug) {
		Serial.print(F(", W: "));Serial.print(receivedCode.wconst);
		Serial.print(F(", P: ")); Serial.print(receivedCode.par);
		Serial.print(F(", P1: ")); Serial.print(receivedCode.min1Period);
		Serial.print(F("-")); Serial.print(receivedCode.max1Period);
		Serial.print(F(", P2: ")); Serial.print(receivedCode.min2Period);
		Serial.print(F("-")); Serial.print(receivedCode.max2Period);
		Serial.println("");
	}
#endif
	msgCnt++;
	return;
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

	queueItem item;
	item.address = receivedCode.address;
	item.channel = receivedCode.channel;
	item.value = (float)receivedCode.temperature/10;
	sprintf(item.label,"temperature");
	sprintf(item.brand,"auriol");
	sprintf(item.action,"sensor");
	sprintf(item.type,"json");
	QueueChain::addQueue(item, NULL);

#if STATISTICS==1
	if (debug >= 1) {
		Serial.print(F(" ! Auriol Cs: ")); Serial.print(receivedCode.csum);
		Serial.print(F(", N1: ")); Serial.print(receivedCode.n1,BIN);
		Serial.print(F(", N2: ")); Serial.print(receivedCode.n2,BIN);	

		Serial.print(F(", 1P: ")); Serial.print(receivedCode.min1Period);
		Serial.print(F("-")); Serial.print(receivedCode.max1Period);
		Serial.print(F(", 2P: ")); Serial.print(receivedCode.min2Period);
		Serial.print(F("-")); Serial.print(receivedCode.max2Period);
		Serial.println("");
	}
#endif
	msgCnt++;
	return;
}
#endif

// --------------------------------------------------------------------------------
// KAKU
// Regular klikaanklikuit devices
// Callback function is called only when a valid code is received.
//
void showKakuCode(KakuCode receivedCode) {

  queueItem item;
  item.gaddr = receivedCode.address;
  item.uaddr = receivedCode.unit;
  sprintf(item.cmd,"kaku");
  sprintf(item.action,"gui");
	
  switch (receivedCode.switchType) {
    case KakuCode::off:
      sprintf(item.val, "off");
	  sprintf(item.message,"!R%dD%dF0", receivedCode.address, receivedCode.unit);
      break;
    case KakuCode::on:
      sprintf(item.val, "on");
	  sprintf(item.message,"!R%dD%dF1", receivedCode.address, receivedCode.unit);
      break;
    case KakuCode::dim:										// The dimlevel worked with in Lampi is 2 * phisical level.
	  if (receivedCode.dimLevelPresent) {
		sprintf(item.val, "%2d", receivedCode.dimLevel*2);
		sprintf(item.message,"!R%dD%dFdP%d", receivedCode.address, receivedCode.unit, receivedCode.dimLevel*2);
	  }
	  else {
		Serial << F("showKakuCode:: no dim value") << endl;
	  }
    break;
  }
  Serial << F("! WT440:: ") << msgCnt << F(" A: ") << receivedCode.address << F(":") << receivedCode.unit << endl;
  QueueChain::addQueue(item, NULL);

  msgCnt++;
  
  if (receivedCode.groupBit) {
    Serial.print(F(" G "));
  } 
  else {
    Serial.print(F(" "));
    Serial.print(receivedCode.unit);
  }
  
  if (debug>1) {
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
	Serial.println();
	Serial.flush();
  }

}


// --------------------------------------------------------------------------------
// LIVOLO
//
#if R_LIVOLO==1
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
#endif

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
#if STATISTICS==1
		Serial.print(F(", 1P: ")); Serial.print(receivedCode.min1Period);
		Serial.print(F("-")); Serial.print(receivedCode.max1Period);
		Serial.print(F(", 3P: ")); Serial.print(receivedCode.min3Period);
		Serial.print(F("-")); Serial.print(receivedCode.max3Period);
#endif
	}
	Serial.println("");
	msgCnt++;
}
#endif


// --------------------------------------------------------------------------------
// REMOTE
// General receiver for old Kaku, Impulse/Action, etc.
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
		Serial.print(F(" ! Remote ")); 
#if STATISTICS==1
		Serial.print(F(", P: ")); Serial.print(period);
#endif	 
	}
	Serial.println("");
	msgCnt++;
}

