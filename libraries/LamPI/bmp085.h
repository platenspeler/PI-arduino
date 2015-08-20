/* BMP085 Extended Example Code
  by: Jim Lindblom
  SparkFun Electronics
  date: 1/18/11
  updated: 2/26/13
  license: CC BY-SA v3.0 - http://creativecommons.org/licenses/by-sa/3.0/
  
  Get pressure and temperature from the BMP085 and calculate 
  altitude. Serial.print it out at 115200 baud to serial monitor.

*/


#if defined(ARDUINO) && ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif
#include "LamPI.h"

#define BMP085_ADDRESS 0x77  // I2C address of BMP085

class BMP085 {
	
public:
  BMP085();

  //Public Functions
  int Calibration();
  short GetTemperature();
  long GetPressure();

  //Public Variables


  // Use these for altitude conversions
  const float p0 = 101325;     // Pressure at sea level (Pa)


private:
  //Private Functions
  int ReadInt(unsigned char address);
  char Read(unsigned char address);
  unsigned int ReadUT();
  unsigned long ReadUP();
  
  
  //Private Variables

  // Calibration values
int ac1;
int ac2; 
int ac3; 
unsigned int ac4;
unsigned int ac5;
unsigned int ac6;
int b1; 
int b2;
int mb;
int mc;
int md;

// b5 is calculated in bmp085GetTemperature(...), this variable is also used in bmp085GetPressure(...)
// so ...Temperature(...) must be called before ...Pressure(...).
long b5; 	
	
};
