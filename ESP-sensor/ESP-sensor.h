//
// Author: Maarten Westenberg (mw12554@hotmail.com)
// Version: 1.7.5

// Date: 2015-11-22
//

// WiFi definitions. If WIFI not set, we do not send Wifi messages to Host
// and only use the Serial Gateway.
// We use regular TCP port 5002, as it is the bidirectional port used by LamPI

#define _HOST "YourIP"
#define _PORT 8080
#define _SSID "YouSSID"
#define _PASS "YourSSIDpassword"

#define _ADDR 100	// Address to send to the LamPI node daeemon

#define BAUDRATE 115200
#define ONE_WIRE_BUS 2


// Wired Sensors. Often these sensors are mutually exclusive:
// For example if you have a BMP085 there will in general not be a SHT21/HTU21D
#define S_DALLAS 1      //Temperature, pin 7 used for DALLAS
#define S_BMP085 1      //Airpressure_and_temperature I2C sensor
#define S_HTU21D 1      //Temperature_and_Humidity I2C sensor
#define S_BH1750 0      //Luminescense I2C sensor
#define S_BATTERY 0     //Internal voltage reference pin A0
#define S_DS3231 0      //RTC I2C sensor      
#define S_PIR 0         //PIR sensor connected to pin 6