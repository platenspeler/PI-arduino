//
// LamPI specific definitions that apply to ALL Library functions
//

#define BAUDRATE 115200

#ifndef STATISTICS
#define STATISTICS 0
#endif


// Enable Receivers
#define R_KOPOU  0
#define R_LIVOLO 0
#define R_ACTION 1
#define R_KAKU 1

// define Sensors. Often these sensors are mutually exclusive:
// If you have a BMP085 there will not be a SHT21/HTU21D

#define S_BMP085 1
#define S_HTU21D 0
#define S_WT440 1
#define S_AURIOL 0
