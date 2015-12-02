/*
 *  This sketch sends data via HTTP GET requests to data.sparkfun.com service.
 *
 *  You need to get streamId and privateKey at data.sparkfun.com and paste them
 *  below. Or just customize this script to talk to other HTTP servers.
 *
 */

#include "ESP-sensor.h"
#include <ESP8266WiFi.h>
#include <Base64.h>
#include <LamPI_ESP.h>							// PIN Definitions
#include <Time.h>
#include <Wire.h>

#include <InterruptChain.h>

#if S_DALLAS==1
#include "OneWireESP.h"
#include "DallasTemperature.h"
OneWire oneWire(A_ONE_WIRE);
DallasTemperature sensors(&oneWire);		// Pass our oneWire reference to Dallas Temperature. 
int numberOfDevices; 						// Number of temperature devices found
#endif

#if S_HTU21D==1
#include <HTU21D.h>
HTU21D myHumidity;							// Init Sensor(s)
#endif

#if S_BMP085==1
#include <bmp085.h>
BMP085 bmp085;
#endif

//
// Network
//
const char* ssid     = "_SSID";
const char* password = "_PASS";

//
// Host
//
const char* host = "_HOST";			// LamPI master node
const int httpPort = _PORT;					// LamPI node.js websocket

//
// Message fields
//
const char* streamId   = "sensor";			// message type
const char* privateKey = "NodeMCU1.0_100";	// Unique identifier (for access)
const char* brand = "esp8266";				// Identification of Node type
const char* address = "_ADDR";				// Unique identifier for LamPI, in database.cfg
const char* channel = "2";

//
// Others
//
int debug;
int msgCnt = 0;				// tcnt
int value = 0;				// Value

// ------------------------------------------------------------
//
//
void setup() {
	Serial.begin(115200);
	delay(10);

	Serial.println();						// Because of garbage output by ESP12 at boot
	Serial.println();

#if S_DALLAS==1
	// DS18B20 initialisation
	sensors.begin();
	numberOfDevices = sensors.getDeviceCount();
	Serial.print("! #Dallas: "); Serial.println(numberOfDevices);
#endif
#if S_HTU21D==1
	myHumidity.begin(); 
	myHumidity.setResolution(0);
	// onCodec(ONBOARD);
	if (myHumidity.readHumidity() == 998) Serial.println(F("! No HTU21D")); // First read value after starting does not make sense.  
#endif
#if S_BMP085==1
	bmp085.begin(); 
	Serial.print(F("I2C SDA: "));
	Serial.print(SDA);
	Serial.print(F(", SCL: "));
	Serial.println(SCL);
	//onCodec(ONBOARD);
	if (bmp085.Calibration() == 998) Serial.println(F("! No BMP085"));	// OnBoard
#endif

	// We start by connecting to a WiFi network 
	Serial.print("Connecting to ");
	Serial.println(ssid);
	int retries = 0;
	
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
		retries++;
		if (retries == 50) {
			retries = 0;
			Serial.println();
		}
	}
	Serial.println("");
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
}

// ------------------------------------------------------------
//
//
void loop() {

	Serial.println();
	Serial.println(F("Loop:: Resetting transmitter"));
	digitalWrite(A_TRANSMITTER, LOW);			// Reset Transmitter
	tRestart(100);
	
	delay(5000);
	
#if S_DALLAS==1
  	uint8_t ind;
	DeviceAddress tempDeviceAddress; 			// We'll use this variable to store a found device address

	++msgCnt;
	sensors.requestTemperatures();
	for(int i=0; i<numberOfDevices; i++)		// For every 1-wire device found on the bus
	{
		Serial.print(F("ds18b20 device "));
		Serial.print(i);
		Serial.print(", id: ");
		
		// Search the wire for address
		if(sensors.getAddress(tempDeviceAddress, i))
		{
			// Address bus is tempDeviceAddress, channel 0
			// For LamPI, print in different format compatible with WiringPI
			for (uint8_t j= 0; j < 7; j++)			// Skip int[7], this is CRC
			{
				if (j<1) ind=j; else ind=7-j;
				if (j==1) Serial.print("-");
				if (tempDeviceAddress[ind] < 16) Serial.print("0");
				Serial.print(tempDeviceAddress[ind], HEX);
			}
			Serial.print(F(", temperature: "));
			// It responds almost immediately. Let's print out the data
			float tempC = sensors.getTempC(tempDeviceAddress);
			Serial.print(tempC,1);
			Serial.println();
			
			if (WifiTransmit(address, channel, "temperature", (String)tempC) < 0) {
				Serial.println(F("ERROR WifiTransmit"));
			}
		} 
		//else ghost device! Check your power requirements and cabling
		else {
			Serial.print(F("Device not OK: "));
			Serial.println(i);
		}
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
	}
	else if (debug>=1) Serial.println(F(" ! No HTU21"));
#endif

#if S_BMP085==1		
	// BMP085 or BMP180
	short temperature = bmp085.GetTemperature();
	if ((temperature != 998) && (temperature != 0)){
		long pressure = bmp085.GetPressure();
		float altitude = (float)44330 * (1 - pow(((float) pressure/bmp085.p0), 0.190295));
		Serial.print(F("! BMP temp: "));					// Address bus 77, channel 0
		Serial.print(temperature, DEC);
		Serial.print(F(", Airpressure: "));
		Serial.print(pressure, DEC);
		Serial.print(F(",Altitude: "));
		Serial.print(altitude, 2);
		if (debug>=1) {
			Serial.print(F(" ! bmp: t: "));
			Serial.print((float)(temperature/10), 1);
		}
		Serial.println();
		float value = (float)temperature/10;			// We need the first decimal as well
		WifiTransmit(address, "2", "temperature", (String)(value) );
		WifiTransmit(address, "2", "airpressure", (String)(pressure/100) );
	}
#endif	
	
	delay(45000);				// XXX should be close to 2 minutes
}

// ------------------------------------------------------------
// Funcction transmitting Wifi messages to host
//
int WifiTransmit(String address, String channel, String label, String value) {
	Serial.print(F("connecting to "));
	Serial.println(host);

	// Use WiFiClient class to create TCP connections
	WiFiClient client;

	if (!client.connect(host, httpPort)) {
		Serial.println(F("ERROR connection failed"));
		return (-1);
	}

	// We now create a URI for the request
	String url = "/";
	url += streamId;
	url += "?private_key=";
	url += privateKey;
	url += "&tcnt=";
	url += msgCnt;
	url += "&brand=";
	url += brand;
	url += "&type=";
	url += "json";
	url += "&address=";
	url += address;
	url += "&channel=";
	url += channel;
	url += "&";
	url += label;
	url += "=";
	url += value;
	Serial.print(F("Requesting URL: "));
	Serial.println(url);

	// This will send the request to the server
	client.print(String("GET ") + url + " HTTP/1.1\r\n" +
      "Host: " + host + "\r\n" +
      "Connection: close\r\n\r\n");
	delay(10);

	// Read all the lines of the reply from server and print them to Serial
	while (client.available()) {
		String line = client.readStringUntil('\r');
		Serial.print(line);
	}
	Serial.println();							// Close output of server
	Serial.println(F("closing connection"));
	msgCnt++;
	return(0);
}

//
// Restart the Transmitter. Just in case the transmitter will go to sleep (some do)
//
void tRestart(int ms) {
	int txPin = A_TRANSMITTER;
	digitalWrite(txPin, HIGH);
	delayMicroseconds(100);
	digitalWrite(txPin, LOW);
	delayMicroseconds(ms);
}