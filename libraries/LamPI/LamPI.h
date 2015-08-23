// --------------------------------------------------------------------
// LamPI specific definitions that apply to ALL Library functions
// Author: Maarten Westenberg
// Date: August 19, 2015
// Version 1.5
//
// This file contains a number of compile-time settings that can be set on (=1) or off (=0)
// The disadvantage of compile time is minor compared to the memory gain of not having
// too much code compiled and loaded on you Arduino.
// --------------------------------------------------------------------

#define BAUDRATE 115200

// If you enable this setting, in debug mode all kind off
// min/max timers will be displayed
#define STATISTICS 0

// Enable Receivers
#define R_KOPOU  0
#define R_LIVOLO 1
#define R_ACTION 1
#define R_KAKU 1

// define and enable Sensors. Often these sensors are mutually exclusive:
// For example if you have a BMP085 there will not be a SHT21/HTU21D
#define S_BMP085 0
#define S_HTU21D 1
#define S_WT440 1
#define S_AURIOL 1


// ==========================================================================
// General definitions used in all library programs
// Do not change below this line

// Devices Define
#define KAKU    0
#define ACTION  1
#define BLOKKER 2
#define KAKUOLD 3
#define ELRO    4
#define LIVOLO  5
#define KOPOU   6

// Sensors Define
#define ONBOARD 0
#define WT440  1
#define OREGON 2
#define AURIOL 3
#define CRESTA 4

