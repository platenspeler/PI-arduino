// --------------------------------------------------------------------
// LamPI specific definitions that apply to ALL Library functions
// Author: Maarten Westenberg
// Date: August 23, 2015
// Version 1.6
//
// --------------------------------------------------------------------




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

// Which Arduino Pins are used for what
// Data wire is plugged into port 10 on the Arduino
#define A_RECEIVER 2
#define S_TRANSMITTER 8
#define A_SCL 5
#define A_SDA 4
#define ONE_WIRE_BUS 10
