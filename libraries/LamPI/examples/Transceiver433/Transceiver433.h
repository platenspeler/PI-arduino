//
// Author: Maarten Westenberg
// Version: 1.7
// Date: 2015-08-25
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

// Enable Receivers. 
// If you set to 0, the receiver functions will NOT be compiled and included.
#define R_KOPOU  0
#define R_LIVOLO 1
#define R_ACTION 1
#define R_KAKU 1

// define and enable Sensors. Often these sensors are mutually exclusive:
// For example if you have a BMP085 there will not be a SHT21/HTU21D
#define S_DALLAS 1
#define S_BMP085 1
#define S_HTU21D 1
#define S_WT440 1
#define S_AURIOL 1
#define S_BH1750 1