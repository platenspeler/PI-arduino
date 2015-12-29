/* --------------------------------------------------------------------------------
* Code for LamPI Gateway based on ESP8266 chip. 
*
* (c) M. Westenberg (mw12554@hotmail.com)
*
* Based on contributions and work of many:
*	IDE code for ESP8266
* 	Randy Simons (Kaku and InterruptChain)
*	Spc for Livolo
*	SPARCfun for HTU21 code and BMP85
* 	and others (Wt440 protocol) etc.
*
* NOTE: You can enable/disable modules in the ESP-Gateway.h file
*		Sorry for making the code in this file so messy :-) with #if
*
* ---------------------------------------------------------------------------------
*/

/* DESCRIPTION

  This file implements the ESP8266 side of the LamPI Gateway.
  setup:
  1. The program connects to a Wifi access point (Parameters specified in ESP-gateway.ino)
  2. The program then connects to a server and opens the socket connection (port 5002).
  3. Depending on the settings in the .h file sensors are initialized and receiver handlers
	with their interrupt routines are attached to the interrupt pin.
	
  loop:
  1. Manage connection, if lost reestablish it
  2. In the main loop() we read the socket for incoming (READ) commands from the server daemon,
	and after decoding we send messages over the air (433MHz) to devices or sensors
  3. After an elapsed time report the values of sensors that are enabled (in .h file)
  
  interrupt:
  1. If messages are recognized on one of the emables receivers, the corresponding handler function
	is called (defined in setup), and values are reported back to daemon over the socket.

  So: baudrate and supported protocol must be set corecly in the ArduinoGateway.h file
  And these must match the lampi_arduino.h definitions in file on ~/receivers/arduino on Raspberry
  
*/

// Please look at http://platenspeler.github.io/HardwareGuide/ESP8266/ESP-Gateway/esp-gateway.html
// for definitions of the message format for the ESP Gateway.
//
#define VERSION " ! V. 1.7.6, 151220"

// General Include
//
#include "ESP-Gateway.h"			// Specifies what modules to load for compiling
#include "LamPI_ESP.h"				// PIN Definitions
#include <ESP8266WiFi.h>
#include <wifiQueue.h>				//http://github.com/platenspeler
#include <ESP.h>
#include <Base64.h>

#include <Wire.h>
#include <Time.h>					//http://playground.arduino.cc/Code/Time
#include "OneWireESP.h"				// Special, renamed, version for ESP
#include <Streaming.h>          	//http://arduiniana.org/libraries/streaming/
#include <ArduinoJson.h>
#include <pins_arduino.h>

// Transmitters Include
// Be careful not to include transmitters that initialize structures in their .h file
// which will negatively impact the memory
//
#include <kakuTransmitter.h>
#include <RemoteTransmitter.h>		//Randy Simons
#include <livoloTransmitter.h>		//http://github.com/platenspeler
#include <kopouTransmitter.h>		//http://github.com/platenspeler
#include <quhwaTransmitter.h>		//http://github.com/platenspeler
//
// Receivers include. 
// Some cannot be commented as the preprocessor need the argument type of functions
// that are defined inside an #if (for unknown reasons, complex parameters in functions 
// will then not be recognized)
//
#include <wt440Receiver.h>			//http://github.com/platenspleler
#include <kakuReceiver.h>
#include <livoloReceiver.h>
#include <RemoteReceiver.h>			
#include <kopouReceiver.h>			//http://github.com/platenspeler
#include <RemoteReceiver.h>			// for Action receivers
#include <auriolReceiver.h>			//http://github.com/platenspeler
#include <quhwaReceiver.h>			//http://github.com/platenspeler
#include <InterruptChain.h>			// Randy Simons

// Use WiFiClient class to create TCP connections
// For the gateway we will keep the connection open as long as we can
// and re-estanblish when broken.
// And we create a management server too to get statistics and timings
WiFiClient client;
#if A_SERVER==1
WiFiServer server(SERVERPORT);
#endif

//
// Sensors Include
//
#if S_DALLAS==1
#include <OneWireESP.h>
#include <DallasTemperature.h>
OneWire oneWire(A_ONE_WIRE);		// pin  is set in LAMPI_ESP.h 
DallasTemperature sensors(&oneWire);
int numberOfDevices; 				// Number of temperature devices found
#endif

#if S_HTU21D==1
#include <HTU21D.h>
HTU21D myHumidity;					// Init HTU Sensor(s)
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
#include "DS3232RTC.h"				//http://github.com/JChristensen/DS3232RTC
#endif

#if STATISTICS==1
stat myStat;
#endif

unsigned long myTime;				// fill up with millis();
boolean debug;						// If set, more informtion is output to the serial port
unsigned int msgCnt=1;				// Not unique, as at some time number will wrap.
boolean wifiReConnect;
int sensorLoops;					// Loop() counter
uint8_t retries = 0;

// Use a bit array (coded in long) to keep track of what protocol is enabled.
// Best is to make this dynamic and not compile time. However,only the Arduino Mega has enough memory
// to run all codecs with all their declarations without problems.
//
unsigned long codecs;				// must be at least 32 bits for 32 positions. Use long instead of array.
QueueChain wQueue;					// Wifi Queue

String OutString;					// Used for debugging, both Serial and on-line

// ********************************************************************************
// SETUP
// Setup all sensor routines and the 433 Interrupts
//
// ********************************************************************************

void setup() {

	wdt_enable(255);				// Watchdog time reset 200 ms, hope it works
	wifiReConnect=false;
	msgCnt = 0;
	debug = DEBUG;					// Define in .h file
	sensorLoops=0;
	codecs = 0;
	
	// Create a transmitter using digital pin A_TRANSMITTER (default is GPIO16) to transmit,
	// with a period duration of 260ms (default), repeating the transmitted
	// code 2^3=8 times.
	pinMode(BUILTIN_LED, FUNCTION_3); 
	pinMode(BUILTIN_LED, OUTPUT);
	
	pinMode(A_TRANSMITTER, OUTPUT);
	digitalWrite(A_TRANSMITTER, LOW);

	pinMode(A_RECEIVER, FUNCTION_0);
	pinMode(A_RECEIVER, INPUT);
	digitalWrite(A_RECEIVER, LOW);

	// Setup WiFi client connection
	if (WifiConnect( (char *) _SSID, (char *)_PASS) < 0) {
		OutString += F("Error Wifi network SSID: ");
		OutString += _SSID;
		OutString += ":";
		OutString += _PASS;
		//printConsole(OutString,1);
	}

	// Client connect
	if (HostConnect( (char *) _HOST, _PORT) < 0) {
		OutString += F("Error Host Connection ");
		OutString += _HOST;
		OutString += ":"; 
		OutString += _PORT;
		//printConsole(OutString,1);
	}
	
	if (debug>=1) {
		Serial.begin(BAUDRATE);		// As fast as possible for bus
		OutString += F("! debug: "); 
		OutString += debug; 
		printConsole(OutString,1);
	}
	
#if S_DS3231==1
	setSyncProvider(RTC.get);
	OutString += F("! RTC Sync");
    if (timeStatus() != timeSet) OutString += F(" FAIL!"); else OutString += F(" OK");
	printConsole(OutString,1);
	onCodec(ONBOARD);
	onCodec(C_DS3231);
#endif

	// it should be a low as possible to avoid the client.available() 
	// call to block interrupts etc.
	// Getting and handling a message of around 120 characters on reasonable speed
	// should not take more time than this.
	client.setTimeout(5);			// XXX set below 50 msec, and as low as possible

#if A_SERVER==1
	//
	server.begin();					// Start the server
	OutString += F("Admin Server started on port ");
	OutString += SERVERPORT;
	printConsole(OutString, 1);
#endif
  
	// Initialize receiver on interrupt 0 (= digital pin 2), calls the callback (for example "showKakuCode")
	// after 2 identical codes have been received in a row. (thus, keep the button pressed for a moment)
#if R_KAKU==1
	KakuReceiver::init(-1, 2, showKakuCode);
	OutString += F("KAKU ");
#endif
#if R_ACTION==1
	RemoteReceiver::init(-1, 2, showRemoteCode);
	OutString += F("ACTION ") ;
#endif
#if R_LIVOLO==1
	livoloReceiver::init(-1, 3, showLivoloCode);
	OutString += F("LIVOLO ") ;
#endif
#if R_KOPOU==1
	kopouReceiver::init(-1, 3, showKopouCode);
	OutString += F("KOPOU ") ;
#endif
#if R_QUHWA==1
	quhwaReceiver::init(-1, 3, showQuhwaCode);
	OutString += F("QUHWA ") ;
#endif
#if S_WT440==1
	wt440Receiver::init(-1, 1, showWt440Code);
	OutString += F("WT440 ") ;
#endif
#if S_AURIOL==1
	auriolReceiver::init(-1, 2, showAuriolCode);
	OutString += F("AURIOL ") ;
#endif
	printConsole(OutString,1);

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
	OutString += F("! #Dallas: "); 
	OutString += numberOfDevices;
	printConsole(OutString,1);
	if (numberOfDevices > 0) { onCodec(ONBOARD); onCodec(C_DALLAS); }
#endif
#if S_HTU21D==1
	myHumidity.begin(); 
	myHumidity.setResolution(0);
	if (myHumidity.readHumidity() == 998) {
		OutString += F("! No HTU21D");
		printConsole(OutString,1); 					// First read value after starting does not make sense.  
	}
	else { onCodec(ONBOARD); onCodec(C_HTU21D); };
#endif
#if S_BMP085==1
	bmp085.begin(); 
	if (bmp085.Calibration() == 998) { 
		OutString += F("! No BMP085");
		printConsole(OutString,1); 						// OnBoard
	}
	else { onCodec(ONBOARD); onCodec(C_BMP085); };
#endif
#if S_BH1750==1
	LightSensor.begin();
	LightSensor.SetAddress(Device_Address_H);			//Address 0x23 or 0x5C
	LightSensor.SetMode(Continuous_H_resolution_Mode2);
	if (LightSensor.GetLightIntensity() == (int) -1) {
		OutString += F("! No BH1750");
		printConsole(OutString,1);
	}
	else { onCodec(ONBOARD); onCodec(C_BH1750); }
#endif

	printCodecs();
	if (debug>=1) {
		OutString += F("! Pin A_RECEIVER is pin ");
		OutString += A_RECEIVER;
		OutString += F(" is interrupt: ");
		OutString += digitalPinToInterrupt(A_RECEIVER);
		printConsole(OutString,1);
		OutString += F("! Pin A_TRANSMITTER is pin ");
		OutString += A_TRANSMITTER;
		printConsole(OutString,1);
		OutString += F("! Heap: ");
		OutString += ESP.getFreeHeap();
		printConsole(OutString,1);
	}
	myTime = millis();
#if STATISTICS==1
	myStat = {0};
#endif
}

// ********************************************************************************
// MAIN LOOP 
// As we have limited possibilities for async handling of Serial and Wifi we will
// use .available and loop constantly
// We insert delay() calls in this code as it blocks the user-level WiFi stuff
// so that our receiver interrupts will be handled as well.
// Importants is that we do not loose incoming Wifi messages so we have to call client.availble
// on a regular basis, on the other side our 433receiver must not miss too much traffic and
// be able to send it to the server (using the same WiFi routines).
// As we keep the loop() short the WiFi background processes will also run on a regular basis.
// ********************************************************************************

void loop() {
  // We can safely do below, as the transmitter is not used in interrupt routines, but by queueHandler.
  digitalWrite(A_TRANSMITTER, LOW);					// make sure digital transmitter pin is low when not used

  // WiFI has/takes priority over Serial commands (probably depreciated in next release).  
  // Are we connected to WiFI network, if not do so.
  
  if (WiFi.status() != WL_CONNECTED) {
	digitalWrite(BUILTIN_LED,HIGH);
	OutString += F("Network reconnect ");
	OutString += retries;
	OutString += "\n";
	//delay(1);
	if (retries == 0) {								// START WLAN connect. Do only once and wait for answer
		WiFi.begin(_SSID, _PASS);
		delay(1);
		//wdt_reset();
		retries = 1;
	}
	else {
		digitalWrite(BUILTIN_LED,HIGH);
		delay(250);
		digitalWrite(BUILTIN_LED,LOW);
		delay(250);
		//OutString += "." ;
		if (retries == 50) {
		//	OutString += "\n";
			retries = 1;
		}
		retries++;
	}	
	return;											// Short Loop
  }
  else {
	retries = 0;									// If WLAN connected reset retries
  }

  // Check whether we're still connected to the server
  if (!client.connected()) {						// If HOST connection is lost
	OutString += F("! Reconnecting to host ...");
	OutString += _HOST;
	printConsole(OutString,1);
	if (HostReconnect((char *)_HOST, _PORT) < 0){ 
		// Reconnect not yet successful
		delay(1000);
	}
	else {
		OutString += F("! Client re-connected");
		digitalWrite(BUILTIN_LED,LOW);
		printConsole(OutString,1);
		wifiReConnect=false;
	}
	return;
  }
  
  // Check for client incoming messages from daemon and process to 433 transmitter
  //delay(1);
  WifiReceive();									// Read the Wifi buffers

  if (debug >= 2) {
	  delay(1);
	  QueueChain::printQueue();
  }

  // Handle the WiFi server part o fthis sketch. Mainly used for administration of the node
 #if A_SERVER==1
  //delay(1);
  WifiServer();
#endif

  // If there are any new messages on the queue, take them off and process
  handleQueue();  // Read if interrupts put something in QUEUE
  
  // Handle Sensors reading (based on time). Lowest priority, lowest in chain. 
  // As long as we read these messages once in a while we're OK.
  //delay(1);
  readSensors();									// Better for callbacks if there are sleeps
}//loop


// ***************************** Codecs Admin *************************************
// 
// This section defines the functions needed to enable/disable the various codecs
// of the sensors and the transmitter/receivers.
//
// ********************************************************************************


// --------------------------------------------------------------------------------
// Switch a codec to on.
//
void onCodec (byte codec) {
	codecs |= ( (unsigned long) 0x0001 << codec	);	// AND

  // We support dynamic enablement and disable of sensors and devices
}

// --------------------------------------------------------------------------------
// We really should disable interrupts for codecs that are in the interrupt chain too.
//
void offCodec (byte codec) {
	codecs &= ~( (unsigned long) 0x0001 << codec );	// NAND, no effect for 0, but for 1 bit makes 0

	// We support dynamic enablement and disable of sensors and devices
}

// --------------------------------------------------------------------------------
//
//
void setCodec (byte codec, byte val) {
  if (val == 0) offCodec(codec);
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
// LIST CODECS
// Used by print codecs and by WifiServer to list all codecs used
//
int listCodecs(String *s){
	*s = "! Codecs enabled: ";
	if ((codecs >> KAKU) & 0x0001) *s += "KAKU ";
	if ((codecs >> ACTION) & 0x0001) *s += "ACTION ";
	if ((codecs >> BLOKKER) & 0x0001) *s += "BLOKKER ";
	if ((codecs >> KAKUOLD) & 0x0001) *s += "KAKOLD ";
	if ((codecs >> ELRO) & 0x0001) *s += "ELRO ";
	if ((codecs >> KOPOU) & 0x0001) *s += "KOPOU ";
	if ((codecs >> LIVOLO) & 0x0001) *s += "LIVOLO ";
	if ((codecs >> QUHWA) & 0x0001) *s += "QUWAH ";
	
	if ((codecs >> ONBOARD) & 0x0001) *s += "ONBOARD ";
	if ((codecs >> WT440) & 0x0001) *s += "WT440 ";
	if ((codecs >> OREGON) & 0x0001) *s += "OREGON ";
	if ((codecs >> AURIOL) & 0x0001) *s += "AURIOL ";
	if ((codecs >> CRESTA) & 0x0001) *s += "CRESTA ";
	
	if ((codecs >> C_DALLAS) & 0x0001) *s += "DALLAS ";
	if ((codecs >> C_HTU21D) & 0x0001) *s += "HTU21D ";
	if ((codecs >> C_BMP085) & 0x0001) *s += "BMP085 ";
	if ((codecs >> C_BH1750) & 0x0001) *s += "BH1750 ";
	if ((codecs >> C_DS3231) & 0x0001) *s += "DS3231 ";
	if ((codecs >> C_PIR) & 0x0001) *s += "PIR ";
	if ((codecs >> C_BATTERY) & 0x0001) *s += "BATTERY ";
	if ((codecs >> C_DHT) & 0x0001) *s += "DHT ";

	return(0);
}


// --------------------------------------------------------------------------------
// PRINT CODECS
// To the Serial output
//
void printCodecs() {
	String s;
	listCodecs(&s);
	printConsole(s,1);
	return ;
}


// ************************* SENSORS PART *****************************************
//
// This sction defines the sensor functions used by lamPI
// Every sensor will be read (if defined by #if) and its value sent by the socket
//
// ********************************************************************************


// --------------------------------------------------------------------------------
// Read ONBOARD sensors
//
// The list of supported onBoard sensors is limited, but can be easily expanded.
// Based on the most common used sensors that are enabled at compile-time the function
// it will calculate is value and send it to the daemon socket currently connected.
//
void readSensors() {
	// Only start reading sensors when enough time hase elapsed.
	int delta = millis() - myTime;
	if ((delta < 0) || (delta > 63000)){			// 63 seconds, so avoiding collisions with other cron jobs
	  myTime = millis();
	  // We use sensorLoops to record how many times we came here before timer expired
	  if (debug >= 2) {
		OutString += F("Sensor Loops: ");
		OutString += sensorLoops;
		OutString += F(", Avg: ");
		OutString += (63000/sensorLoops);
		OutString += F(" ms");
		printConsole(OutString,2);
		OutString += F(", Heap: ");
		OutString += ESP.getFreeHeap();
		printConsole(OutString,2);
	  }
	  sensorLoops = 0;
		
#if S_DALLAS==1
		// Dallas sensors (can be more than 1) have channel codes 3 and above!
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
				if (debug>=2) {
					OutString += F("! DS18B20 dev ");
					OutString += i;
					printConsole(OutString,2);
				}
				//delay(1);
				if (SensorTransmit(_ADDR, (3+i), (char *)"esp8266" ,(char *) "temperature", tempC) < 0) {
					OutString += F("ERROR SensorTransmit");
					printConsole(OutString,1);
				}
			} 
			//else ghost device! Check your power requirements and cabling
		}
#endif

#if S_HTU21D==1
		// HTU21 or SHT21 has channel code 0
		float humd = myHumidity.readHumidity();
		float temp = myHumidity.readTemperature();
		if (((int)humd != 999) && ((int)humd != 998 )) {	// Timeout (no sensor) or CRC error
			if (debug>=2) {
				Serial << F(" ! HTU21 T: ") << temp << endl;
			}
			if (SensorTransmit(_ADDR, 0, (char *)"esp8266" ,(char *)"temperature", temp) < 0) {
				Serial.println(F("ERROR SensorTransmit"));
			}
			if (SensorTransmit(_ADDR, 0, (char *)"esp8266" ,(char *)"humidity", humd) < 0) {
				Serial.println(F("ERROR SensorTransmit"));
			}
			msgCnt++;
		}
		else if (debug>=1) { OutString += F(" ! No HTU21"); printConsole(OutString,1); };
#endif

#if S_BMP085==1		
	  // BMP085 or BMP180 Temperature/Airpressure sensors has channel code 1
	  if (bmp085.Calibration() != 998) {					// Hope we can call this function without exception
		short temperature = bmp085.GetTemperature();
		if ((temperature != 998) && (temperature != 0)) {
			long pressure = bmp085.GetPressure();
			//float altitude = (float)44330 * (1 - pow(((float) pressure/bmp085.p0), 0.190295));
			if (debug>=2) {
				Serial << F("! BMP: t: ") << (float)(temperature/10) << endl;
				delay(1);									// Too make sure that this message gets printed
			}
			if (SensorTransmit(_ADDR, 1, (char *)"esp8266" ,(char *)"temperature", (float)temperature/10) < 0) {
				Serial.println(F("ERROR SensorTransmit"));
			}
			delay(1);
			if (SensorTransmit(_ADDR, 1, (char *)"esp8266", (char *)"airpressure", (pressure/100)) < 0) {
				Serial.println(F("ERROR SensorTransmit"));
			}
			msgCnt++;
		}
		else { Serial.println(F(" ! Error reading BMP085")); }
	  }
	  else if (debug>=2) { OutString += F(" ! No BMP085"); printConsole(OutString,2); }
#endif

#if S_BH1750==1
// Luminescense sensor has channel code 2
		uint16_t lux = LightSensor.GetLightIntensity();			// Get Lux value
		if (lux != (int) -1) {
			if (SensorTransmit(_ADDR, 2, (char *)"esp8266", (char *)"luminescense", (float)lux) < 0) {
				if (debug>=1) Serial.println(F("ERROR SensorTransmit"));
			}
			msgCnt++;
		}
#endif
	}
	else { sensorLoops++; }
}




// ********************** WIFI SECTiON ********************************************
// This section contains the Wifi functions that we use for the ESP gateway.
//
// NOTE: We cannot use blocking functions or the ESP watchdog will reset
//	the chip and we get exceptions. So make sure to return to loop as soon as possible
// and in next loopt we'll reconnect again if necessary.
//
// NOTE2: Make sure that interrupt routines return as soon as possibe too.
//	So these service routines must NOT reset connections or so as it takes too much time.
//
// ********************************************************************************

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
	int ledStatus = LOW;
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		digitalWrite(BUILTIN_LED, ledStatus); // Write LED high/low
		ledStatus = (ledStatus == HIGH) ? LOW : HIGH;
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
#if STATISTICS==1
	myStat.lastWifiConnect=millis();
#endif
	digitalWrite(BUILTIN_LED, LOW);
	return(0);
}

// --------------------------------------------------------------------------------
//
// Loop until connected again
//
int HostReconnect(char *host, int httpPort){

	wifiReConnect=true;									// Make sure we do not re-enter this routine
	if (WiFi.status() != WL_CONNECTED) {				// If not connected to a network
		return(-1);
	}
	if (!client.connected()) {
		digitalWrite(BUILTIN_LED,HIGH);
		delay(10);
		if (!client.connect(host, httpPort)) {
			OutString += F("! ERROR reconnect ");
			OutString += host;
			OutString += F(":");
			OutString += httpPort;
			return -1;
		}
#if STATISTICS==1
		myStat.lastWifiConnect = millis();
#endif
	}
	else {
		digitalWrite(BUILTIN_LED,LOW);
		OutString += F("! Reconnected to host");
		printConsole(OutString,1);
	}
	wifiReConnect=false;
	return(0);
}


// --------------------------------------------------------------------------------
// Function transmitting Onboard Sensor Wifi messages to host
//
int SensorTransmit( uint32_t address, uint8_t channel, char *brand, char *label, float value) {
	char tbuf[A_MAXBUFSIZE];
	int ival = (int) value;						// Make integer part
	int fval = (int) ((value - ival)*10);		// Fraction

	sprintf (tbuf,
	"{\"tcnt\":\"%d\",\"type\":\"json\",\"action\":\"sensor\",\"brand\":\"%s\",\"address\":\"%lu\",\"channel\":\"%d\",\"%s\":\"%d.%d\"}"
	, msgCnt, brand, address, channel, label, ival, fval);

	// We cannot block the program in interrupt whith reconnections.
	// Therefore only is connected we're able to send the message.
	// Just hope reconnected will not happen too often.
	
	if (client.connected()) {
#if STATISTICS==1
		myStat.lastWifiWrite=millis();			// Update statistics with this transmission
#endif
		client.write((const char *)tbuf, strlen(tbuf));

		if (debug>=1)
			OutString += F("SEND: ");
			OutString += " <";
			OutString += strlen(tbuf);
			OutString += "> ";
			OutString += address;
			OutString += ":";
			OutString += channel;
			OutString += ", brand: ";
			OutString += brand;
			OutString += ", ";
			OutString += label;
			OutString += ": ";
			OutString += ival;
			OutString += ".";
			OutString += fval;
			printConsole(OutString,1);
		msgCnt++;
	}
	else {
		OutString += F("SensorTransmit:: Not connected");
		printConsole(OutString,1);
		delay(3);
		// XXX This is tricky! But is the received loop() does not get time we have to/
		//wdt_reset();
		wifiReConnect=true;
	}
	return(0);
}

// ---------------------------------------------------------------------
// SENSOR QUEUE
// Take a sensor structure off the queue and send it to the daemon over the
// communication socket.
//
int sensorQueue(queueItem qi) {
	char tbuf[A_MAXBUFSIZE];
	int ival = (int) qi.value;					// Make interger part
	int fval = (int) ((qi.value - ival)*10);	// Fraction
	// Copy to string
	sprintf (tbuf,
		"{\"tcnt\":\"%d\",\"type\":\"json\",\"action\":\"sensor\",\"brand\":\"%s\",\"address\":\"%lu\",\"channel\":\"%d\",\"%s\":\"%d.%d\"}"
		, msgCnt, qi.brand, qi.address, qi.channel, qi.label, ival, fval);
	// Send to WiFi Transmit
	if (client.connected()) {
#if STATISTICS==1
		myStat.lastWifiWrite=millis();
#endif
		client.write((const char *)tbuf, strlen(tbuf));
		if (debug>=1) {
			OutString += F("SEND: ");
			OutString += " <";
			OutString += strlen(tbuf);
			OutString += "> ";
			OutString += qi.address;
			OutString += ":";
			OutString += qi.channel;
			OutString += ", brand: ";
			OutString += qi.brand;
			OutString += ", ";
			OutString += qi.label;
			OutString += ": ";
			OutString += ival;
			OutString += ".";
			OutString += fval;
			// Here add brand and values
			//OutString += tbuf;
			printConsole(OutString,1);
		}
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
	char tbuf[A_MAXBUFSIZE];
	char message[16];
	
	// Copy to string
	sprintf (tbuf,
		"{\"tcnt\":\"%d\",\"type\":\"json\",\"action\":\"%s\",\"cmd\":\"%s\",\"gaddr\":\"%lu\",\"uaddr\":\"%d\",\"val\":\"%s\",\"message\":\"%s\"}"
		, msgCnt, qi.action, qi.cmd, qi.gaddr, qi.uaddr, qi.val, qi.message);
	// Send to WiFi Transmit
	if (client.connected()) {
#if STATISTICS==1
		myStat.lastWifiWrite=millis();
#endif
		// Handset Addresses cannot(!) be in the same range as the LamPI used addresses.
		// The gateway will simply not send such messages from 433MHz back to WiFi socket of server.
		if (qi.gaddr<=A_RESERVED_ADDRESS) { 
			if (debug>=1) Serial << F("Address ") << qi.gaddr << F(" reserved to LamPI") << endl;
			return(-1);
		}
		client.write((const char *)tbuf, strlen(tbuf));
		if (debug>=1)
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
	while ((QueueChain::processQueue(&qi)) >= 0) {
		// We have a valid action the queue
		// Maybe make the action an enumerated type ...
		if (strcmp(qi.action, "sensor")==0) {
			// sensor
			sensorQueue(qi);
		}
		else if (strcmp(qi.action, "gui")==0) {
			// Could be gui
			if (debug >=2) {
				Serial << F("handleQueue:: Gui read") << endl;
			}
			deviceQueue(qi);
		}
		else if (strcmp(qi.action, "handset")==0) {
			// or handset
			if (debug >= 2) {
				Serial << F("handleQueue:: Handset read") << endl;
			}
			deviceQueue(qi);
		}
		else {
			Serial << F("handleQueue:: ERROR unknow action") << endl;
		}
	}
	return(0);
}


// --------------------------------------------------------------------------------
// WIFI RECEIVE
//
// This function deals with receiving from the daemon socket and do some actions
// Daemon commands are json encoded
//
int WifiReceive() {
	int ind;

	// Read all the lines of the reply from server and print them to Serial
	//
	if (!client.connected()) 	{
		digitalWrite(BUILTIN_LED, HIGH);
		OutString += F("! WifiReceive: not connected");
		printConsole(OutString,1);
		return(-1);
	}
	else {
		//delay(5);
	}
	
	// Reading until a \r\n does not work and leads to timeout.
	// Server will only send JSON messages which are packed in {}
	if (client.available()) {
		// Read until } character, but as not included as part of result, append immediately
		String line = client.readStringUntil('}') + "}"; 
#if STATISTICS==1
		myStat.lastWifiRead=millis();
#endif

		// Now decode the JSON string received from the server
		StaticJsonBuffer<256> jsonBuffer;
		JsonObject& root = jsonBuffer.parseObject(line.c_str());
		
		if (!root.success()) {
			OutString += F(" ! ERROR WifiReceive:: Json Decode");
			printConsole(OutString,1);
			return(-1);
		}
		//delay(1);
		const char * action = root["action"];
		
		if (strcmp(action,"alarm")==0) {
			OutString += F(" ! ERROR WifiReceive:: ALARM received");
			printConsole(OutString,1);
			//delay(5);
			return(-1);
		}
		int tcnt = root["tcnt"];
		const char * type  = root["type"];
		const char * cmd = root["cmd"];						// Only if this token is present
		unsigned int gaddr = root["gaddr"];
		byte uaddr = root["uaddr"];
		const char * val = root["val"];
		const char * message = root["message"];
		
		OutString += F("READ:  <");
		OutString += line.length();
		OutString += "> ";
		OutString += gaddr;
		OutString += ":";
		OutString += uaddr;
		OutString += ", ";
		OutString += cmd;
		OutString += ", value: ";
		OutString += val;
		printConsole(OutString,1);
		
		// Parse and transmit KAKU message	
		if (strcmp(cmd, "kaku")==0) {
			int value = atoi(val) /2 ;						// XXX Dimlevel of devices is 0-15, socket LamPI level 0-31
			KakuTransmitter transmitter(A_TRANSMITTER, 260, 3);
			if (val[0] == 'o') {
				if (val[1]=='n') transmitter.sendUnit(gaddr, uaddr, true);
				else if (val[1]=='f') transmitter.sendUnit(gaddr, uaddr, false);
				else { OutString += F(" ! wifiReceive:: Unknown button command"); printConsole(OutString,1); }
			}
			else if (value == 0) {
				transmitter.sendUnit(gaddr, uaddr, false);
			} 
			else if (value >= 1 && value <= 15) {
				transmitter.sendDim(gaddr, uaddr, value);
			} 
			else {
				OutString += F(" ! ERROR dim not between 0 and 15!");
				printConsole(OutString,1);
			}
		} 
		// This is an action command
		else if (strcmp(cmd, "action")==0) {
			OutString += F(" ! action cmd");
			printConsole(OutString,1);
			ActionTransmitter atransmitter(A_TRANSMITTER, 195, 3);		// Timing 195us pulse
			if (strcmp(val, "on")==0) atransmitter.sendSignal(gaddr,uaddr,true);
			else atransmitter.sendSignal(gaddr,uaddr,false);
		}
		// Livolo
		else if (strcmp(cmd, "livolo")==0) {
			OutString += F(" ! livolo cmd");
			printConsole(OutString,1);
			Livolo livolo(A_TRANSMITTER);
			livolo.sendButton(gaddr, uaddr);
		}
		// zwave
		else if (strcmp(cmd, "zwave")==0) {
			OutString += F(" ! Zwave cmd Ignored");
			printConsole(OutString,2);
		} 
		else {
			OutString += F(" ! Command not recognized: ");
			printConsole(OutString,1);
		}
	}
//XXX	InterruptChain::enable(digitalPinToInterrupt(A_RECEIVER));			// XXX Set interrupts on
	return(0);
}

// --------------------------------------------------------------------------------
// WIFI SERVER
//
// This funtion implements the WiFI Webserver (very simple one). The purpose
// of this server is to receive simple admin commands, and execute these
// results are sent back to the web client.
// Commands: DEBUG, ADDRESS, IP, CONFIG, CODECS, KAKU, GETTIME, SETTIME
//
#if A_SERVER==1
void WifiServer() {

	String response;
	char *dup, *pch;
	char *cmd, *arg;
	WiFiClient clnt = server.available();
	if (!clnt) return;

	//clnt.setTimeout(500);
	delay(1);
		
	if (debug >=2) { OutString += F("WifiServer new client"); printConsole(OutString,2); }
	if (clnt.available()) {
		// Do work
		delay(15);
		String request = clnt.readStringUntil('\r');

		// So the syntax on URL is ?<request>=<value>
		dup = strdup(request.c_str() );						// duplicate as we're changing it
		for (int i=0; dup[i]; i++) dup[i] = toupper(dup[i]);// convert to uppercase
		delay(1);
		pch = strtok(dup, " /?");							// This should be "GET"
		pch = strtok (NULL, " /?:="); cmd = pch;			// cmd, eg DEBUG
		pch = strtok(NULL, " /:=");	arg = pch;				// Afrument 1 or so
		
		if (debug >=1) {
			OutString += F("ADMIN: ");
			OutString += cmd;
			OutString += " ";
			OutString += arg;
			printConsole(OutString, 1);
		}
			
		// If there is no argument, we only want to display the current value?
		// In that case, argument arg will proably be "HTTP"

		if (strcmp(cmd, "CONFIG")==0) { 
			response +="<h1>Print Config:</h1>";
			response +="<br>Sensor Adress: "; response+=_ADDR; 
			response +="<br>IP Address: "; response+=(IPAddress)WiFi.localIP()[0]; response+="."; response+=(IPAddress)WiFi.localIP()[1]; response+="."; response+=(IPAddress)WiFi.localIP()[2]; response+="."; response+=(IPAddress)WiFi.localIP()[3];
			response +="<br>Codecs: "; String s; listCodecs(&s); response+=s;
			response +="<br>ESP is alive since "; response+=printTime(1); 
			response +="<br>Current time is    "; response+=printTime(millis()); 
			response +="<br><br>";
#if STATISTICS==1
			
			response +="<table style=\"max_width: 100%; min-widt: 40%; border: 1px solid black; border-collapse: collapse;\" class=\"config_table\">";
			response +="<tr>";
			response +="<th style=\"background-color: green; color: white;\">Item</th>";
			response +="<th style=\"background-color: green; color: white;\">Time</th>";
			response +="</tr>";
			response +="<tr><td style=\"border: 1px solid black;\">Last Wifi    Read</td><td style=\"border: 1px solid black;\">"; response +=printTime(myStat.lastWifiRead); response+="</tr>";
			response +="<tr><td style=\"border: 1px solid black;\">Last Wifi    Write</td><td style=\"border: 1px solid black;\">"; response +=printTime(myStat.lastWifiWrite); response+="</tr>";
			response +="<tr><td style=\"border: 1px solid black;\">Last Wifi    Reconnect</td><td style=\"border: 1px solid black;\">"; response +=printTime(myStat.lastWifiConnect); response+="</tr>";
			response +="<tr><td>&nbsp</td><td> </tr>";
			response +="<tr><td style=\"border: 1px solid black;\">Last Debug   Write</td><td style=\"border: 1px solid black;\">"; response +=printTime(myStat.lastDebugWrite); response+="</tr>";
			response +="<tr><td style=\"border: 1px solid black;\">Last Device  Read</td><td style=\"border: 1px solid black;\">"; response +=printTime(myStat.lastSensorRead); response+="</tr>";
			response +="<tr><td style=\"border: 1px solid black;\">Last Device  Write</td><td style=\"border: 1px solid black;\">"; response +=printTime(myStat.lastSensorWrite); response+="</tr>";
			response +="<tr><td>&nbsp</td><td> </tr>";
			response +="<tr><td style=\"border: 1px solid black;\">Last Sensor  WT440</td><td style=\"border: 1px solid black;\">"; response +=printTime(myStat.lastSensorWT440);response+="</tr>";
			response +="<tr><td style=\"border: 1px solid black;\">Last Sensor  AURIOL</td><td style=\"border: 1px solid black;\">"; response +=printTime(myStat.lastSensorAURIOL);response+="</tr>";
			response +="<tr><td>&nbsp</td><td> </tr>";
			response +="<tr><td style=\"border: 1px solid black;\">Last Handset KAKU</td><td style=\"border: 1px solid black;\">"; response +=printTime(myStat.lastSensorKAKU);response+="</tr>";
			response +="<tr><td style=\"border: 1px solid black;\">Last Handset ACTION</td><td style=\"border: 1px solid black;\">"; response +=printTime(myStat.lastSensorACTION);response+="</tr>";			
			response +="<tr><td style=\"border: 1px solid black;\">Last Handset LIVOLO</td><td style=\"border: 1px solid black;\">"; response +=printTime(myStat.lastSensorLIVOLO);response+="</tr>";
			response +="</table>";
#endif
		}
		// These can be used as a single argument
		if (strcmp(cmd, "DEBUG")==0) {								// Set debug level 0-2
			debug=atoi(arg); response+=" debug="; response+=arg;
		}
		if (strcmp(cmd, "ADDRESS")==0) {							// Sensor address to use in LamPI (hint: Make larger than 100)
			response+=" address="; response+=_ADDR;
		}
		if (strcmp(cmd, "IP")==0) {									// List local IP address
			response+=" local IP="; 
			response+=(IPAddress) WiFi.localIP()[0]; response += ".";
			response+=(IPAddress) WiFi.localIP()[1]; response += ".";
			response+=(IPAddress) WiFi.localIP()[2]; response += ".";
			response+=(IPAddress) WiFi.localIP()[3];
		}
		if (strcmp(cmd, "CODECS")==0) { 							// List all codecs in use
			String s; listCodecs(&s); 
			response += s; 
		}
		if (strcmp(cmd, "KAKU")==0) { 								// Send a KAKU message to device
			KakuTransmitter transmitter(A_TRANSMITTER, 260, 3);
			int gaddr = atoi(arg);
			pch = strtok(NULL, " /:="); int uaddr = atoi(pch);
			pch = strtok(NULL, " /:="); int value = atoi(pch);
			
			response += "KAKU "; response += gaddr; response += " ";
			response += uaddr; response += " ";	response += pch ;
			
			if (pch[0] == 'O') {
				if (pch[1]=='N') transmitter.sendUnit(gaddr, uaddr, true);
				else if (pch[1]=='F') transmitter.sendUnit(gaddr, uaddr, false);
				else  {response +="! WifiServer:: Unknown KAKU command"; response+=pch; }
			}
			else if (value == 0) {
				transmitter.sendUnit(gaddr, uaddr, false);
			} 
			else if (value >= 1 && value <= 15) {
				transmitter.sendDim(gaddr, uaddr, value);
			} 
			else {
				OutString += F(" ! ERROR dim not between 0 and 15!");
				printConsole(OutString,1);
			}
		}
		if (strcmp(cmd, "GETTIME")==0) { response += "gettime tbd"; }	// Get the local time
		if (strcmp(cmd, "SETTIME")==0) { response += "settime tbd"; }	// Set the local time
		if (strcmp(cmd, "SYSTEM")==0) { 							// List system parameters that are useful
			response += "<br>Free Heap: "; response += ESP.getFreeHeap();
			response += "<br>Chip ID  : "; response += ESP.getChipId();
		}
		
		// Return the response. This part is equal for all Server commands
		clnt.println("HTTP/1.1 200 OK");
		clnt.println("Content-Type: text/html");
		clnt.println(""); //  do not forget this one
		clnt.println("<!DOCTYPE HTML>");
		clnt.println("<HTML><HEAD>");
		clnt.println("<TITLE>ESP8266 Node Management</TITLE>");
		clnt.println("</HEAD>");
		clnt.println("<BODY>");
		
		//clnt.print("Request : "); clnt.println(request);
		clnt.println(response);
		clnt.println("<br><br>");
		clnt.println("Click <a href=\"/CONFIG\">here</a> to show Config statistics<br>");
		clnt.println("Click <a href=\"/CODECS\">here</a> show Codecs<br>");
		clnt.println("Click <a href=\"/SYSTEM\">here</a> show System info<br>");
		clnt.println("Debug level: "); clnt.println(debug); clnt.println(" set to: ");
		clnt.println(" <a href=\"/DEBUG=0\">0</a>");
		clnt.println(" <a href=\"/DEBUG=1\">1</a>");
		clnt.println(" <a href=\"/DEBUG=2\">2</a><br>");

		clnt.println("</BODY></HTML>");
		free(dup);						// free the memory used
		delay(5);
	}
	if (debug >= 2) { OutString += F("WifiServer close"); printConsole(OutString,2); }

}
#endif

// ------------------------------------------------------------------------------------
// printTime
// Only when RTC is present we print real time values
// t contains number of milli seconds since system started that the event happened.
// So a value of 100 wold mean that the event took place 1 minute and 40 seconds ago
String printTime(unsigned long t) {
	String response;
	String Days[7]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
#if S_DS3231==1
	if (t==0) return(" -none- ");
	// now() works in seconds since 1970
	time_t eventTime = now() - ((millis()-t)/1000);
	response += Days[weekday(eventTime)-1];
	response += " ";
	byte _hour = hour(eventTime);
	if (_hour < 10) response += "0";
	response += _hour;
	response +=":";
	byte _minute = minute(eventTime);
	if (_minute < 10) response += "0";
	response += _minute;
	return (response);
#else
	response += t/1000;
	response += " seconds"
	return (response);
#endif

}

// ------------------------------------------------------------------------------------
// PRINT CONSOLE MESSAGES
// The s is the string to be printed, 
// d is the minimal debug level necessary to print this message
int printConsole(String s, int d){

#if STATISTICS==1
	myStat.lastDebugWrite=millis();			// Update statistics with this transmission
#endif
	if ((s.length()+40) >= A_MAXBUFSIZE) {
		//Serial << F("printConsole too long: ") << s << endl;
		return(-1);
	}
	if (client.connected()) {
		if (debug>=d) {
			char tbuf[A_MAXBUFSIZE];
			sprintf (tbuf,
				"{\"tcnt\":\"%d\",\"type\":\"json\",\"action\":\"debug\",\"cmd\":\"logs\",\"message\":\"%s\"}"
				, msgCnt, s.c_str() );
			client.write((const char *)tbuf, strlen(tbuf));
		}

		if (debug>=1){
			Serial << F("# ") << s << endl;
		}
		msgCnt++;
	}
	else {
		if (debug>=d)
			Serial << F("# ") << " <" << s.length() << "> " << s.c_str() << endl;
	}
	delay(1);
	s = "";
	OutString = "";
	return(0);
}

// ************************ 433 TRANSMITTER PART **********************************
//
//
//
// ********************************************************************************

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
	quhwa.sendButton(group, unit);
	OutString += F("! Quhwa:: G: "); 
	OutString += group;
	OutString += F(", U: "); 
	OutString += unit;
	Outstring += F(", L: ");
	OutString += level;
	printConsole(OutString, 1);
}
#endif



// *********************** 433 RECEIVER STUFF BELOW *******************************
//
// This is the section where all the specific 433MHz handling is done.
// It contains all the callback routines used in the interrupts.
//
// ********************************************************************************


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
#if STATISTICS==1
	myStat.lastSensorRead=millis();
	myStat.lastSensorWT440=millis();
#endif
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
			OutString += F("showWT440Code:: Unknown opcode: ");
			OutString += receivedCode.wconst;
			printConsole(OutString,1);
	}
#if STATISTICS==1
	if (debug>=2) {
		OutString += F(", W: "); OutString += receivedCode.wconst;
		OutString += F(", P: "); OutString += receivedCode.par;
		OutString += F(", P1: "); OutString += receivedCode.min1Period;
		OutString += F("-"); OutString += receivedCode.max1Period;
		OutString += F(", P2: "); OutString += receivedCode.min2Period;
		OutString += F("-"); OutString += receivedCode.max2Period;
		OutString += ("\n");
		printConsole(OutString,2);
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
#if STATISTICS==1
	myStat.lastSensorRead=millis();
	myStat.lastSensorAURIOL=millis();
#endif
	QueueChain::addQueue(item, NULL);

#if STATISTICS==1
	if (debug >= 2) {
		OutString += F(" ! Auriol Cs: "); OutString += receivedCode.csum;
		OutString += F(", N1: "); OutString += receivedCode.n1,BIN;
		OutString += F(", N2: "); OutString += receivedCode.n2,BIN;	
		OutString += F(", 1P: "); OutString += receivedCode.min1Period;
		OutString += F("-"); OutString += receivedCode.max1Period;
		OutString += F(", 2P: "); OutString += receivedCode.min2Period;
		OutString += F("-"); OutString += receivedCode.max2Period;
		printConsole(OutString,2);
	}
#endif
	msgCnt++;
	return;
}
#endif

// --------------------------------------------------------------------------------
// KAKU
// Regular handset for klikaanklikuit devices
// Callback function is called only when a valid code is received.
//
void showKakuCode(KakuCode receivedCode) {

  queueItem item;
  item.gaddr = receivedCode.address;
  item.uaddr = receivedCode.unit;
  sprintf(item.cmd,"kaku");
  sprintf(item.action,"handset");
	
  switch (receivedCode.switchType) {
    case KakuCode::off:
      sprintf(item.val, "off");
	  sprintf(item.message,"!A%dD%dF0", receivedCode.address, receivedCode.unit);
      break;
    case KakuCode::on:
      sprintf(item.val, "on");
	  sprintf(item.message,"!A%dD%dF1", receivedCode.address, receivedCode.unit);
      break;
    case KakuCode::dim:										// The dimlevel worked with in Lampi is 2 * physical level.
	  if (receivedCode.dimLevelPresent) {
		sprintf(item.val, "%2d", receivedCode.dimLevel*2);
		sprintf(item.message,"!A%dD%dFdP%d", receivedCode.address, receivedCode.unit, receivedCode.dimLevel*2);
	  }
	  else {
		OutString += F("showKakuCode:: no dim value");
		printConsole(OutString,1);
	  }
    break;
  }
  msgCnt++;
  
  if (receivedCode.groupBit) {
	OutString += F(" G ");
    printConsole(OutString,1);							// No idea yet what to do with this. Not used in lamPI
  } 
  else if (debug>=2) {
    OutString += F(" ");
    OutString += receivedCode.unit;
	printConsole(OutString,2);
  }
#if STATISTICS==1
	myStat.lastSensorRead=millis();
	myStat.lastSensorKAKU=millis();
#endif
  if (debug >=2) {
	OutString += F("! KAKU:: ");
	OutString += msgCnt;
	OutString += F(" A: ");
	OutString += receivedCode.address;
	OutString += F(":");
	OutString += receivedCode.unit;
	printConsole(OutString,2);
  }
  QueueChain::addQueue(item, NULL); 
}


// --------------------------------------------------------------------------------
// LIVOLO
//
#if R_LIVOLO==1
void showLivoloCode(livoloCode receivedCode) {
	queueItem item;
	item.gaddr = receivedCode.address;
	item.uaddr = receivedCode.unit;
	sprintf(item.cmd,"livolo");
	sprintf(item.action,"handset");
	sprintf(item.type,"json");
	sprintf(item.val,"%d",receivedCode.level);
	sprintf(item.message,"!A%dD%dF%d",item.gaddr,item.uaddr,receivedCode.level);
#if STATISTICS==1
	myStat.lastSensorRead=millis();
	myStat.lastSensorLIVOLO=millis();
#endif
	QueueChain::addQueue(item, NULL);
	OutString += F(" ! Livolo ");
	if (debug >=2) {
#if STATISTICS==1
		OutString += F(", P1: "); OutString += receivedCode.min1Period;
		OutString += F("-"); OutString += receivedCode.max1Period;
		OutString += F(", P3: "); OutString += receivedCode.min3Period;
		OutString += F("-"); OutString += receivedCode.max3Period;
#endif
		printConsole(OutString,2);
	}
}
#endif

// --------------------------------------------------------------------------------
// KOPOU switches
//
#if R_KOPOU==1
void showKopouCode(kopouCode receivedCode) {
	queueItem item;
	item.gaddr = receivedCode.address;
	item.uaddr = receivedCode.unit;
	sprintf(item.cmd,"kopou");
	sprintf(item.action,"handset");
	sprintf(item.type,"json");	
	sprintf(item.val,"%d",receivedCode.level);
	sprintf(item.message,"!A%dD%dF%d",item.gaddr,item.uaddr,receivedCode.level);
	if (debug >= 2) {
		Serial.print(F(" ! Kopou ")); 
#if STATISTICS==1
		Serial.print(F(", P: ")); Serial.print(receivedCode.period);
		Serial.print(F(", P1: ")); Serial.print(receivedCode.minPeriod);
		Serial.print(F("-")); Serial.print(receivedCode.maxPeriod);
#endif
		Serial.println("");
	}
	msgCnt++;
}
#endif

// --------------------------------------------------------------------------------
// QUHWA doorbell protocol
//
#if R_QUHWA==1
void showQuhwaCode(quhwaCode receivedCode) {
	queueItem item;
	item.gaddr = receivedCode.address;
	item.uaddr = receivedCode.unit;
	sprintf(item.cmd,"quhwa");
	sprintf(item.action,"handset");
	sprintf(item.type,"json");	
	sprintf(item.val,"%d",receivedCode.level);
	sprintf(item.message,"!A%dD%dF%d",item.gaddr,item.uaddr,receivedCode.level);
	if (debug >= 2) {
		Serial.print(F(" ! Quhwa "));
#if STATISTICS==1
		Serial.print(F(", 1P: ")); Serial.print(receivedCode.min1Period);
		Serial.print(F("-")); Serial.print(receivedCode.max1Period);
		Serial.print(F(", 3P: ")); Serial.print(receivedCode.min3Period);
		Serial.print(F("-")); Serial.print(receivedCode.max3Period);
#endif
		Serial.println("");
	}
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
void showRemoteCode(uint32_t receivedCode, unsigned int period) {
	int i;
	byte level = 0;
	byte unit = 0;
	short address = 0;
	byte codec = 1;

	// Unfortunately code is Base3
	unsigned long code = receivedCode;
#if STATISTICS==1
	myStat.lastSensorRead=millis();
	myStat.lastSensorACTION=millis();
#endif
	queueItem item;
	sprintf(item.action,"handset");

	if ( (period > 120 ) && (period < 180 ) ) {			// Action codec
		sprintf(item.cmd,"action");
		for (i=0; i<2; i++) {
			level = level * 10;
			level += code % 3;
			code = code / 3;
		}
		item.value = (float) level;
		// two bits, either 02 or 20, the 0 determines the value
		if (level == 20) { 
			level = 0; 
			sprintf(item.val, "on");
		}
		else { 
			level = 1; 
			sprintf(item.val, "on");
		}
		
		// 5 bits, 5 units. The position of the 0 determines the unit (0 to 5)	
		for (i =4; i >= 0; i--) {
			if ((code % 3) == 0) unit= i;
			code =  code / 3;
		}
		item.uaddr = unit;
		
		// Position of 1 bit determines the address values
		for (i=0; i < 5; i++) {
			if ((code % 3) == 1) address += ( B00001<<i );
			code =  code / 3;
		}
		item.gaddr = address;
		sprintf(item.message,"!A%dD%dF%d",address,unit,level);
		QueueChain::addQueue(item, NULL);
	}
	else {
		OutString += F("! showRemoteCode:: NOT handled, period: ");
		OutString += period;
		printConsole(OutString,1);
		delay(5);
	}
	
	if (debug >= 2) {	
		// Following code should be equal for ALL receivers of this type
		OutString += F("REMOTE period: "); 
		OutString += period; 
		printConsole(OutString,1);
	}
	msgCnt++;
}

