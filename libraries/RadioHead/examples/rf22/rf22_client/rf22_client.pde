// rf22_client.pde
// -*- mode: C++ -*-
// Example sketch showing how to create a simple messageing client
// with the RH_RF22 class. RH_RF22 class does not provide for addressing or
// reliability, so you should only use RH_RF22 if you do not need the higher
// level messaging abilities.
// It is designed to work with the other example rf22_server
// Tested on Duemilanove, Uno with Sparkfun RFM22 wireless shield
// Tested on Flymaple with sparkfun RFM22 wireless shield
// Tested on ChiKit Uno32 with sparkfun RFM22 wireless shield

#include <SPI.h>
#include <RH_RF22.h>

// Singleton instance of the radio driver
RH_RF22 rf22;
long tcnt;
long errs;

void setup() 
{
  Serial.begin(9600);
  tcnt = 0;
  errs = 0;
  if (!rf22.init()) {
    Serial.println("init failed");
  }
  else {
	Serial.println("init OK");
  }
  // Defaults after init are 434.0MHz, 0.05MHz AFC pull-in, modulation FSK_Rb2_4Fd36
}

void loop()
{
  // Send a message to rf22_server
  uint8_t data[RH_RF22_MAX_MESSAGE_LEN];
  sprintf((char *)data, "%d: Hello World", tcnt++);
  
  rf22.send(data, sizeof(data));
  Serial.print(F("rf22 txmit ... "));
  if (! rf22.waitPacketSent(500)) {
	Serial.println(F("timeout"));
	//return;
  }
  else {
	Serial.println(F("success"));
	// Now wait for a reply
	uint8_t buf[RH_RF22_MAX_MESSAGE_LEN];
	uint8_t len = sizeof(buf);

	Serial.print(F("Wait for rf22_server ... "));
	if (rf22.waitAvailableTimeout(400))
	{ 
		// Should be a reply message for us now   
		if (rf22.recv(buf, &len))
		{
			Serial.print(F("reply: "));
			Serial.println((char*)buf);
		}
		else
		{
			Serial.println(F("recv failed"));
		}
	}
	else
	{
		errs++;
		Serial.print(F("No reply, is rf22_server running? errs: "));
		Serial.println(errs);
	}
  }
  delay(1000);
}

