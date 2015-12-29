// --------------------------------------------------------------------
// LamPI specific definitions that apply to ALL Library functions
// Author: Maarten Westenberg
// Date: Dec 01, 2015
// Version 1.7.6
//
// --------------------------------------------------------------------

// Define STATISTICS if we want more information about protocol handling

// ==========================================================================
// General definitions used in all library programs
// Do not change below this line

// Devices Define, for the array that is shared between Arduino and Raspberry
// it enables the Raspberry to see which functions are enabled by THIS connected Arduino
// Encoding type for switches/dimmers:
#define KAKU    0
#define ACTION  1
#define BLOKKER 2
#define KAKUOLD 3
#define ELRO    4
#define LIVOLO  5
#define KOPOU   6
#define QUHWA	7

// Encoding type of Sensors Definitions used for messages sent to Gateway
#define ONBOARD 16
#define WT440  17
#define OREGON 18
#define AURIOL 19
#define LAMPI 20
#define CRESTA 21

// Analog Pin:
//		 0: The ADC analog(!) pin for measuring battery 
#define BATTERY_PIN 0

// Which Digital Arduino Pins are used for what
// Data wire was  port 10 on the Arduino for older DS18b20 gateways
// but since pin 10 is used by RadioHead as well, switched to pin 7!!!
// PIN	
//		 2: Receiver pin for the 433MHz receivers
//		 3: <free> DHT11/DHT21/DHT22?
//		 4: The SDA pin of the I2C bus
//		 5: The SCL clock pin of the I2c bus
//		 6: Pin for detecting PIR motion
//		 7: One wire bus pin. This was(!) pin 10 (so change for old gateways)
//		 8: Transmitter pin of the 433MHz transmitter
//		 9: Pin for RadioHead transceiver SI4432/RF22 and NRF24L01
//		10: Pin for RadioHead transceiver SI4432/RF22 and NRF24L01
//		11: Pin for RadioHead transceiver SI4432/RF22 and NRF24L01
//		12: Pin for RadioHead transceiver SI4432/RF22 and NRF24L01
//		13: Pin for RadioHead transceiver SI4432/RF22 and NRF24L01
#define A_RECEIVER 2
#define A_SHT22
#define A_SDA 4
#define A_SCL 5
#define PIR_PIN 6
#define ONE_WIRE_BUS 7
#define S_TRANSMITTER 8
#define RH_SCLCK	9
#define RH_10	10
#define RH_11	11
#define RH_12	12
#define RH_13	13

