//
// Author: Maarten Westenberg
// Version: 1.7.4
// Date: 2015-11-11
//
// This file contains a number of compile-time settings that can be set on (=1) or off (=0)
// The disadvantage of compile time is minor compared to the memory gain of not having
// too much code compiled and loaded on your Arduino.
//
// ----------------------------------------------------------------------------------------

#define BAUDRATE 115200

// If you enable this setting, in debug mode all kind off
// min/max timers will be displayed
#define STATISTICS 0

// If we deal with an Arduino Mega, we have much more memory available.
// This means we could support ALL receivers and sensors below. So please
// set A_MEGA to 1, also set all other devices and sensors below to 1.
// However be careful with the amount of receivers (R_ ) to enable as parsing
// too many protocols will slow down interrupt handling and may provide unrielable results
#define A_MEGA 1

// Enable Receivers of handsets. 
// If you set to 0, the receiver functions will NOT be compiled and included.
#define R_KOPOU  1
#define R_LIVOLO 1
#define R_ACTION 1
#define R_KAKU 1
#define R_KAKUOLD 0
#define R_BLOKKER 0
#define R_QUHWA 1

// Enable transmitters yes (1) or no (0)
#define T_QUHWA 1

// define and enable wireless Sensors. Only enable these you have them
#define S_WT440 1
#define S_AURIOL 1
#define S_CRESTA 0
#define S_OREGON 0

// Wired Sensors. Often these sensors are mutually exclusive:
// For example if you have a BMP085 there will in general not be a SHT21/HTU21D
#define S_DALLAS 1      //Temperature, pin 7 used for DALLAS
#define S_BMP085 1      //Airpressure_and_temperature I2C sensor
#define S_HTU21D 1      //Temperature_and_Humidity I2C sensor
#define S_BH1750 1      //Luminescense I2C sensor
#define S_BATTERY 0     //Internal voltage reference pin A0
#define S_DS3231 1      //RTC I2C sensor
#define S_PIR 0         //PIR sensor connected to pin 6

