// -------------------------------------------------------------------------
// LamPI specific definitions that apply to ALL Library functions
// Author: Maarten Westenberg (mw12554@hotmail.com)
// Date: Dec 23, 2015
// Version 1.7.7
// -------------------------------------------------------------------------

// =========================================================================
// You may set different pins for digital and analog pins depending on your own 
// board. Be careful, as for the ESP8266 not all pins can be reused just like that.

// Analog Pin:
//		 0: The ADC analog(!) pin for measuring battery 
#define BATTERY_PIN 0

// Which Digital Arduino Pins are used for what
// GPIO	numbers are pin numbers and interrupt numbers in IDE

#define A_RECEIVER 2            // used GPIO2/D4 for 100
#define A_TRANSMITTER 16        // GPIO16
#define A_SDA 4                 // GPIO4 is the default value
#define A_SCL 5                 // GPIO5 is the default value
#define A_PIR X
#define A_DHT X             	// Maybe share with PIR pin?
#define A_ONE_WIRE 12         	// GPIO2 
#define A_LED 1


// ==========================================================================
// General definitions used in all library programs
// Do not change below this line
// In future, this might become an array of boolens to hold all the enabled devices...
// Devices Define, for the array that is shared between Arduino and Raspberry
// it enables the Raspberry to see which functions are enabled by THIS connected Arduino
// Encoding type for switches/dimmers:
#define KAKU    0
#define ACTION  1
#define BLOKKER 2
#define KAKUOLD 3
#define ELRO    4
#define LIVOLO  5
#define KOPOU   6
#define QUHWA	7
// Until 15

// Encoding type of Sensors Definitions used for messages sent to Gateway
#define ONBOARD 16
#define WT440  17
#define OREGON 18
#define AURIOL 19
#define LAMPI 20
#define CRESTA 21
// Until 24

// Codecs
#define C_DALLAS 24
#define C_BMP085 25	// Same as BMP180
#define C_HTU21D 26	// Same as 
#define C_DS3231 27	// Real-Time clock
#define C_BH1750 28	// Luminescence
#define C_BATTERY 29
#define C_PIR 30
#define C_DHT 31	// DHT11,21,22 temperature/humidity Sensors
// Geo/Compass
