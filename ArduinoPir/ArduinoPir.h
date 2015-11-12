//
// Author: Maarten Westenberg
// Version: 1.7.3
// Date: 2015-11-05
//

#define BAUDRATE 115200
// What is the Epic address that we should use for this Sensor
#define OWN_ADDRESS 9

// If you enable this setting, in debug mode all kind off
// min/max timers will be displayed
#define STATISTICS 0

// define and enable Sensors. Often these sensors are mutually exclusive:
// For example if you have a BMP085 there might not be a SHT21/HTU21D
#define S_DALLAS 0
#define S_BMP085 0
#define S_HTU21D 0
#define S_BH1750 0
#define S_PIR 1
#define S_BATTERY 0