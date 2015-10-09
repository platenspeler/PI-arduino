// rf22_server.pde
// -*- mode: C++ -*-
// Example sketch showing how to create a simple messageing server
// with the RH_RF22 class. RH_RF22 class does not provide for addressing or
// reliability, so you should only use RH_RF22  if you do not need the higher
// level messaging abilities.
// It is designed to work with the other example rf22_client
// Tested on Duemilanove, Uno with Sparkfun RFM22 wireless shield
// Tested on Flymaple with sparkfun RFM22 wireless shield
// Tested on ChiKit Uno32 with sparkfun RFM22 wireless shield

#include <SPI.h>
#include <RH_RF22.h>

// Singleton instance of the radio driver
RH_RF22 rf22;
long tcnt;
long loops;

void setup() 
{
  tcnt =1;
  loops = 1;
  Serial.begin(115200);
  if (!rf22.init()) {
    Serial.println("init failed"); 
  }
  else {
	Serial.println(F("init success"));
  }
  // Defaults after init are 434.0MHz, 0.05MHz AFC pull-in, modulation FSK_Rb2_4Fd36
}

void loop()
{
  if (rf22.available())
  {
    // Should be a message for us now   
    uint8_t buf[RH_RF22_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
	loops=0;
    if (rf22.recv(buf, &len))
    {
//      RF22::printBuffer("request: ", buf, len);
      Serial.print("got request: ");
      Serial.println((char*)buf);
//      Serial.print("RSSI: ");
//      Serial.println(rf22.lastRssi(), DEC);
      
      // Send a reply
      char data[RH_RF22_MAX_MESSAGE_LEN];
	  sprintf(data, "%s %d", (char *) buf, tcnt++);
	  
	  rf22.send((uint8_t *)data, sizeof(data));
      rf22.waitPacketSent();
    }
    else
    {
      Serial.println("recv failed");
    }
  }
//  else {
//	Serial.print(".");
//	loops++;
//	if ((loops%40) == 0) Serial.println();
//	delay(100);
//  }
}

