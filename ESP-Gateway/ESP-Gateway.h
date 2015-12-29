//
// Author: Maarten Westenberg
// Version: 1.7.7
// Date: 2015-12-23
//
// This file contains a number of compile-time settings that can be set on (=1) or off (=0)
// The disadvantage of compile time is minor compared to the memory gain of not having
// too much code compiled and loaded on your Arduino.
//
// ----------------------------------------------------------------------------------------

// If you enable this setting, in debug mode all kind off min/max timers will be displayed
#define STATISTICS 1
// Initial value of debug var. Can be hanged using the admin webserver
#define DEBUG 1
#define A_SERVER 1				// Define local WebServer only if this define is set

#define A_MAXBUFSIZE 192		// Must be larger than 128, but small enough to work
#define BAUDRATE 115200			// Works for debug messages to serial momitor (if attached).

// LamPI Daemon definitions. 
// We use regular TCP port 5002, as it is the bidirectional port used by LamPI

#define _HOST "YourDaemonIP"
#define _PORT 5002
#define _SSID "YourWifiSSID"
#define _PASS "YourWifiPassword"
#define _ADDR 102				// Address to send to the LamPI node daeemon
#define A_RESERVED_ADDRESS 200	// All addresses below this are reserved for LamPI (and not for handset use)

// Definitions for the admin webserver

#define SERVERPORT 8080			// local webserver port

// Enable Receivers of handsets. 
// If you set to 0, the receiver functions will NOT be compiled and included.
// However be careful with the amount of receivers (R_ ) to enable as parsing
// too many protocols will slow down interrupt handling and may provide unreliable results
#define R_KOPOU  0
#define R_LIVOLO 1
#define R_ACTION 1
#define R_KAKU 1
#define R_KAKUOLD 0
#define R_BLOKKER 0
#define R_QUHWA 1

// Enable transmitters yes (1) or no (0)
#define T_QUHWA 0

// define and enable wireless Sensors. Only enable these you have them
#define S_WT440 1
#define S_AURIOL 1
#define S_CRESTA 0
#define S_OREGON 0

// Wired Sensors. Often these sensors are mutually exclusive:
// For example if you have a BMP085 there will in general not be a SHT21/HTU21D
#define S_DALLAS 1      		//Temperature, pin 7 used for DALLAS
#define S_BMP085 1				//Airpressure_and_temperature I2C sensor
#define S_HTU21D 1				//Temperature_and_Humidity I2C sensor
#define S_BH1750 1				//Luminescense I2C sensor
#define S_BATTERY 0				//Internal voltage reference pin A0
#define S_DS3231 1				//RTC I2C sensor, leave value to ON=1 even when RTC not present  
#define S_PIR 1					//PIR sensor connected to pin 6
#define S_DHT 0					// Temperature/Humidity Sensors

#if STATISTICS==1
// A lot of sensor and device statistics can be gathered during operation
// as we do not have a live logging connection we need this to inspect the ESP
// funtion at runtime.
struct stat {
	unsigned long lastWifiRead;				// last time we received  Wifi message
	unsigned long lastWifiWrite;
	unsigned long lastWifiConnect;			// If we reconnect to Wifi server, record the timestamp
	unsigned long lastDebugWrite;			// Log messages sent back to the server
	unsigned long lastSensorRead;
	unsigned long lastSensorWrite;
	unsigned long lastSensorBMP085;			// sensors
	unsigned long lastSensorHTU21D;
	unsigned long lastSensorDALLAS;
	unsigned long lastSensorWT440;
	unsigned long lastSensorAURIOL;
	unsigned long lastSensorKAKU;
	unsigned long lastSensorACTION;
	unsigned long lastSensorLIVOLO;
};
#endif