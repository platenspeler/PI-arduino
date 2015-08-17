Arduino Gateway functions
=========================
Arduino examples (executables) and library functions for Lampi
(c) Maarten Westenberg (mw1255 @ hotmail . com)
August 2015

Introduction
------------
In the example directory of the library you find a set of functions that make you use your Arduino as a gateway or remote sensor for the LamPI Home Automation system. Instead of connecting all receivers, transmitters and sensors to the Raspberry in a wired fasion, we connect these to an Arduino which has nothing else on his/her mind and let the Arduino handle all the communication.
The Arduino system communinicates to the Raspberry over USB (gateway) or when used as a Arduino Sensor it communicates over 433MHz using the Epic protocol.

Modes of operation
------------------
When loading one of the programs which are locaed in the example directory one choses the modus of operation of the Arduino system.

1. Arduino Gateway. The Arduino is connected to the Raspberry over USB, and it has an optional HTU21 sensor. It translated all incoming handsets and sensor messages to codes used by the LamPI daemon
2. Arduino Sensor. The Arduino system is tanding alone, and once programmed it is not connected to a Raspberry. It will report HTU21 temperature and humidity information over he air (433 MHZ) to Raspberry systems listening.
3. Arduino Repeater. (*) The Arduino operates as a stand-alone system. It will report sensor values over the air JUST LIKE the Arduino Sensor. But it also has a 433MHz receiver whih allows it to operate as a 433 MHZ repeater for messages too.

(*) Not yet finished