//
// Author: Maarten Westenberg
// Version: 1.7
// Date: 2015-08-25
//

// What is the Epic address that we should use for this Sensor
#define OWN_ADDRESS 5

// If you enable this setting, in debug mode all kind off
// min/max timers will be displayed
#define STATISTICS 0

// define and enable Sensors. Often these sensors are mutually exclusive:
// For example if you have a BMP085 there might not be a SHT21/HTU21D
//	S_DALLAS: Enable 1-wire Dallas temperature sensors
//	S_PIR: Infrared sensor, change the level of a pin

#define S_DALLAS 1
#define S_BMP085 1
#define S_HTU21D 1
#define S_BH1750 1
#define S_PIR 1