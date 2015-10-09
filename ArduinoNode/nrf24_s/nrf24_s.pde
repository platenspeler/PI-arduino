// nrf24_reliable_datagram_server.pde
// -*- mode: C++ -*-
// Example sketch showing how to create a simple addressed, reliable messaging server
// with the RHReliableDatagram class, using the RH_NRF24 driver to control a NRF24 radio.
// It is designed to work with the other example nrf24_reliable_datagram_client
// Tested on Uno with Sparkfun WRL-00691 NRF24L01 module
// Tested on Teensy with Sparkfun WRL-00691 NRF24L01 module
// Tested on Anarduino Mini (http://www.anarduino.com/mini/) with RFM73 module
// Tested on Arduino Mega with Sparkfun WRL-00691 NRF25L01 module

#include <RHReliableDatagram.h>
#include <RH_NRF24.h>
#include <SPI.h>

#define CLIENT_ADDRESS 1
#define SERVER_ADDRESS 2

// Singleton instance of the radio driver
RH_NRF24 driver;
// RH_NRF24 driver(8, 7);   // For RFM73 on Anarduino Mini

// Class to manage message delivery and receipt, using the driver declared above
RHReliableDatagram manager(driver, SERVER_ADDRESS);
unsigned long tcnt = 0;

void setup() 
{
  Serial.begin(115200);
  if (!manager.init())
    Serial.println("init failed");
  else
	Serial.println("init OK");
  // Defaults after init are 2.402 GHz (channel 2), 2Mbps, 0dBm
}

// Dont put this on the stack:
uint8_t data[RH_NRF24_MAX_MESSAGE_LEN];
uint8_t buf[RH_NRF24_MAX_MESSAGE_LEN];

void loop()
{
  if (manager.available())
  {
    // Wait for a message addressed to us from the client
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager.recvfromAck(buf, &len, &from))
    {
      Serial.print("RCV from 0x");
      Serial.print(from, HEX);
      Serial.print(": ");
      Serial.print((char*)buf);

      // Send a reply back to the originator client
	  sprintf((char *)data,"%s, reply: %lu\0",(char *)buf, ++tcnt);
	  
	  if (!manager.sendtoWait(data, strlen((char *)data), from)){
        Serial.print("N");
	  }
	  else {
		Serial.print(tcnt);
	  }
	  Serial.println();
    }
  }
}

