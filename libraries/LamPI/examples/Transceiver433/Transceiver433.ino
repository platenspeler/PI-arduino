/*
* Code for RF remote switch Gateway. 
* Focus on Kaku, wt440, Livolo devices first, more to follow
*
* Version 1.4 beta; 150811
* (c) M. Westenberg (mw12554@hotmail.com)
*
* Based on contributions and work of many:
* 	Randy Simons (Kaku and InterruptChain)
*	Spc for Livolo
*	SPARCfun for HTU21 code and BMP85
* 	and others (Wt440) etc.
*
* Connect the receiver to digital pin 2, sender to pin 8.
* NOTE: You can enable/disable modules in the LamPI.h file
*		Sorry for making the code in this file so messy :-) with #if
*/

// Sensors Include
#include <Arduino.h>
#include <Wire.h>
#include "LamPI.h"		// Set statistics 1 (default), change line in file if no statistics are needed

// Transmitters Include
#include <LamPITransmitter.h>
#include <RemoteTransmitter.h>
#include <livoloTransmitter.h>
#include <kopouTransmitter.h>

// Receivers include
#include <wt440Receiver.h>
#include <kakuReceiver.h>
#include <livoloReceiver.h>
#include <kopouReceiver.h>
#include <RemoteReceiver.h>

//#if S_AURIOL==1
# include <auriolReceiver.h>	// if auriolCode undefined, preprocessor will fail.
//#endif



#if S_HTU21D==1
# include "HTU21D.h"
#endif
#if S_BMP085==1
# include "bmp085.h"
#endif

// Others
#include <InterruptChain.h>

unsigned long time;			// fill up with millis();
int debug;
int  readCnt;				// Character count in buffer
char readChar;				// Last character read from tty
char readLine[32];			// Buffer of characters read
int msgCnt;
unsigned long codecs;		// must be at least 32 bits for 32 positions. Use long instead of array.

// Create a transmitter using digital pin 8 to transmit,
// with a period duration of 260ms (default), repeating the transmitted
// code 2^3=8 times.


//Kopou kopou(8);			// No function or transmitting Kopou yet (not needed)


// Sensors Declarations
#if S_HTU21D==1
  HTU21D myHumidity;		// Init Sensor(s)
#endif

#if S_BMP085==1
  BMP085 bmp085;
#endif

// --------------------------------------------------------------------------------
//
void setup() {
  
  Serial.begin(BAUDRATE);			// As fast as possible for USB bus
  msgCnt = 0;
  readCnt = 0;
  debug = 0;
  codecs = 0;

  // Initialize receiver on interrupt 0 (= digital pin 2), calls the callback (for example "showKakuCode")
  // after 2 identical codes have been received in a row. (thus, keep the button pressed for a moment)

  NewRemoteReceiver::init(-1, 2, showKakuCode);
  RemoteReceiver::init(-1, 2, showRemoteCode);
  livoloReceiver::init(-1, 3, showLivoloCode);
  kopouReceiver::init(-1, 3, showKopouCode);
  wt440Receiver::init(-1, 1, showWt440Code);
#if S_AURIOL==1
  auriolReceiver::init(-1, 1, showAuriolCode);
#endif

  // Change interrupt mode to CHANGE (on flanks)
  InterruptChain::setMode(0, CHANGE);
  
  // Define the interrupt chain
  // The sequence might be relevant, defines the order of execution, put easy protocols first
  // onCodec: First 16 bits are for sensors (+16), last 16 bits for devices,
  // By removing a line below, that type of device will not be scanned anymore :-)  
  //

  InterruptChain::addInterruptCallback(0, RemoteReceiver::interruptHandler); onCodec(ACTION);
  InterruptChain::addInterruptCallback(0, NewRemoteReceiver::interruptHandler); onCodec(KAKU);
  InterruptChain::addInterruptCallback(0, livoloReceiver::interruptHandler); onCodec(LIVOLO);
  InterruptChain::addInterruptCallback(0, kopouReceiver::interruptHandler);	onCodec(KOPOU);
  InterruptChain::addInterruptCallback(0, wt440Receiver::interruptHandler); onCodec(16 + WT440);
#if S_AURIOL==1
  InterruptChain::addInterruptCallback(0, auriolReceiver::interruptHandler); onCodec(16 + AURIOL);
#endif 

  time = millis();

#if S_HTU21D==1
  myHumidity.begin(); onCodec(16 + ONBOARD);
  myHumidity.readHumidity();			// First read value after starting does not make sense.  
#endif
#if S_BMP085==1
  bmp085.begin(); onCodec(16 + ONBOARD);
  if (bmp085.Calibration() == 998) Serial.println(F("No bmp085"));	// OnBoard
#endif
}

// --------------------------------------------------------------------------------
//
void loop() {
  char * pch;

  while (Serial.available()) {
    InterruptChain::disable(0);					// Set interrupts off  
	readChar = Serial.read();					// Read the requested byte from serial
	if (readChar == '\n') {						// If there is a newLine in the input
	  readLine[readCnt]='\0';	  				// Overwrite the \n char, close the string
	  parseCmd(readLine);						// ACTION: Parsing the readLine for actions
	  readLine[0]='\0';
	  readCnt=0;  
	}
	else {
	  readLine[readCnt]=readChar;
	  readCnt++;
	  readLine[readCnt]='\0';
	}
	InterruptChain::enable(0);					// Set interrupts on again
  }//available

  readSensors();								// Better for callbacks if there are sleeps
}

// ***************************** GENERIC ******************************************

void onCodec (byte codec) {
	codecs |= ( (unsigned long) 0x0001 << codec	);		// AND
}

void offCodec (byte codec) {
  codecs &= ~( (unsigned long) 0x0001 << codec );		// NAND, no effect for 0, but for 1 bit makes 0
}

void setCodec (byte codec, byte val) {
  if (val = 0) offCodec(codec);
  else onCodec(codec);
}

// ************************* SENSORS PART *****************************************


// --------------------------------------------------------------------------------
// 
void readSensors() {
	if ((millis() - time) > 63000) {				// 63 seconds, so avoiding collisions with other cron jobs
		time = millis();
#if S_HTU21D==1
		// HTU21 or SHT21
		float humd = myHumidity.readHumidity();
		float temp = myHumidity.readTemperature();
		if (((int)humd != 999) && ((int)humd !=998 )) {	// Timeout (no sensor) or CRC error
			Serial.print(F("< "));
			Serial.print(msgCnt);
			Serial.print(F(" 3 0 40 0 "));		// Address bus 40, channel 0
			Serial.print(temp,1);
			Serial.print(F(" "));
			Serial.println(humd,1);
			msgCnt++;
		}		
#endif
#if S_BMP085==1		
		// BMP085 or BMP180
		short temperature = bmp085.GetTemperature();
		if ((temperature != 998) && (temperature != 0)){
			long pressure = bmp085.GetPressure();
			float altitude = (float)44330 * (1 - pow(((float) pressure/bmp085.p0), 0.190295));
			Serial.print(F("< "));
			Serial.print(msgCnt);
			Serial.print(F(" 3 0 77 0 "));		// Address bus 77, channel 0
			Serial.print(temperature, DEC);
			Serial.print(F(" "));
			Serial.print(pressure, DEC);
			Serial.print(F(" "));
			Serial.print(altitude, 2);
			Serial.println();
			msgCnt++;
		}
#endif
	}
}


// ************************* TRANSMITTER PART *************************************

// --------------------------------------------------------------------------------
// PARSE COMMAND (first part of message)
//
void parseCmd(char *readLine) 
{
  int cnt;
  byte cmd, codec, ret;
  byte adm;
  byte val;
  char *pch;								// Pointer to character position
	
  if (readLine[0] != '>') {
	Serial.println(F("! Error: cmd \">\" "));
	return;
  } else { 
	pch = readLine+1;
  }
  
  pch = strtok (pch, " ,."); cnt = atoi(pch);
  pch = strtok (NULL, " ,."); cmd = atoi(pch);

  switch(cmd) {
    case 0:									// admin
	  pch = strtok (NULL, " ,."); adm = atoi(pch);
	  switch (adm) {
		case 0:	// List device Codecs
			Serial.print(F("< "));
			Serial.print(msgCnt);
			Serial.print(F(" 0 0 "));
			Serial.println(codecs,BIN);
			msgCnt++;
		break;
		case 1:	// List codec value for ONE sensor only
			pch = strtok (NULL, " ,."); codec = atoi(pch);
			pch = strtok (NULL, " ,."); val = atoi(pch);
			if (val == 1) onCodec(codec); else offCodec(codec);
			Serial.print("! set Cod = "); Serial.println(codecs,BIN);
		break;
		case 2:	// Ask for statistics
			Serial.print("! Stat = "); Serial.println("");
		break;
		case 3:	// Debug level
			pch = strtok (NULL, " ,."); debug = atoi(pch);
			if (debug > 2) { debug = 1; } else if (debug < 0) { debug = 0; }
			
			Serial.print(F("< "));
			Serial.print(msgCnt);
			Serial.print(F(" 0 3 "));
			Serial.print(debug);
			if (debug) Serial.print(F(" ! Debug"));
			Serial.println("");
			msgCnt++;
		break;
		default:
			Serial.println(F("! ERROR admin cmd"));
	  }
	break;
	case 1:								// transmit
		pch = strtok (NULL, " ,."); codec = atoi(pch);
		switch (codec) {
			case 0:
				parseKaku(pch);
			break;
			case 1:
				parseAction(pch);
			break;
			case 5:
				parseLivolo(pch);
			break;
			default:
				Serial.println(F("! ERROR: Codec"));
				return;
		}
		msgCnt++;
	break;
	case 2:								// 
		//
	break;
	default:
		Serial.print(F("! Unknown command "));
		Serial.println(cmd);
  }
  msgCnt++;
}

// --------------------------------------------------------------------------------
// Do parsing of Kaku specific command
// print back to caller the received code
//
void parseKaku(char *pch)
{
  int group;
  int unit;
  int level;
  NewRemoteTransmitter transmitter(8, 260, 3);
  pch = strtok (NULL, " ,."); group = atoi(pch);
  pch = strtok (NULL, " ,."); unit = atoi(pch);  
  pch = strtok (NULL, " ,."); level = atoi(pch);
  if (level == 0) {
    transmitter.sendUnit(group, unit, false);
  } 
  else if (level >= 1 && level <= 15) {
    transmitter.sendDim(group, unit, level);
  } 
  else {
    Serial.println(F("! ERROR dim not between 0 and 15!"));
  }
}


// --------------------------------------------------------------------------------
// Do parsing of Action specific command
// print back to caller the received code
//
void parseAction(char *pch)
{
	byte group;
	char unit;
	ActionTransmitter atransmitter(8, 195, 3);
	boolean lvl = false;
	pch = strtok (NULL, " ,."); group = atoi(pch);
	pch = strtok (NULL, " ,."); unit = atoi(pch);  
	pch = strtok (NULL, " ,."); if (atoi(pch) == 1) lvl = true;
	atransmitter.sendSignal(group, unit, lvl);
}

// --------------------------------------------------------------------------------
// Do parsing of Livolo specific command
// print back to caller the received code
//
// We do expect a level parameter for compatibility with other transmitters
//
void parseLivolo(char *pch)
{
	int group;
	int unit;
	char level;
	Livolo livolo(8);
	pch = strtok (NULL, " ,."); group = atoi(pch);
	pch = strtok (NULL, " ,."); unit = atoi(pch);  
	pch = strtok (NULL, " ,."); level = atoi(pch);
	Serial.print(F("! Livolo:: G: ")); Serial.print(group);
	Serial.print(F(", U: ")); Serial.print(unit);
	Serial.print(F(", L: ")); Serial.println(level);
	livolo.sendButton(group, unit);
}



// ************************* RECEIVER STUFF BELOW *********************************

// --------------------------------------------------------------------------------
// WT440
void showWt440Code(wt440Code receivedCode) {
	
	Serial.print(F("< "));
	Serial.print(msgCnt);
	Serial.print(F(" 3 1 "));
	Serial.print(receivedCode.address);
	Serial.print(" ");
	Serial.print(receivedCode.channel);
	Serial.print(" ");
	Serial.print(receivedCode.temperature);
	Serial.print(" ");
	Serial.print(receivedCode.humidity);

	if (debug >= 1) {
		Serial.print(F(" ! WT440:: x: ")); Serial.print(receivedCode.wconst);
		Serial.print(F(", P: ")); Serial.print(receivedCode.par);
# if STATISTICS==1
		Serial.print(F(", P1: ")); Serial.print(receivedCode.min1Period);
		Serial.print(F("-")); Serial.print(receivedCode.max1Period);
		Serial.print(F(", P2: ")); Serial.print(receivedCode.min2Period);
		Serial.print(F("-")); Serial.print(receivedCode.max2Period);
# endif
	}
	Serial.println("");
	msgCnt++;
}

// --------------------------------------------------------------------------------
// AURIOL
// As the preprocessor is gettig wild when S_AURIOL != 1, we for the moment
// have to comment the code out.

#if S_AURIOL==1
//
void showAuriolCode(auriolCode receivedCode) {
	
	Serial.print(F("< "));
	Serial.print(msgCnt);
	Serial.print(F(" 3 3 "));
	Serial.print(receivedCode.address);
	Serial.print(F(" "));
	Serial.print(receivedCode.channel);
	Serial.print(F(" "));
	Serial.print(receivedCode.temperature);
	Serial.print(F(" "));
	Serial.print(receivedCode.humidity);

	if (debug >= 1) {
		Serial.print(F(" ! Auriol Cs: ")); Serial.print(receivedCode.csum);
		Serial.print(F(", N1: ")); Serial.print(receivedCode.n1,BIN);
		Serial.print(F(", N2: ")); Serial.print(receivedCode.n2,BIN);	
# if STATISTICS==1
		Serial.print(F(", P1: ")); Serial.print(receivedCode.min1Period);
		Serial.print(F("-")); Serial.print(receivedCode.max1Period);
		Serial.print(F(", P2: ")); Serial.print(receivedCode.min2Period);
		Serial.print(F("-")); Serial.print(receivedCode.max2Period);
# endif
	}
	Serial.println("");
	msgCnt++;
}
#endif

// --------------------------------------------------------------------------------
// KAKU
// Callback function is called only when a valid code is received.
//
void showKakuCode(NewRemoteCode receivedCode) {

  // Print the received code. 2 for received codes and 0 for codec of Kaku
  Serial.print(F("< "));
  Serial.print(msgCnt);
  Serial.print(F(" 2 0 "));
  Serial.print(receivedCode.address);
  msgCnt++;
  
  if (receivedCode.groupBit) {
    Serial.print(F(" G "));
  } 
  else {
    Serial.print(F(" "));
    Serial.print(receivedCode.unit);
  }
  switch (receivedCode.switchType) {
    case NewRemoteCode::off:
      Serial.print(F(" off"));
      break;
    case NewRemoteCode::on:
      Serial.print(F(" on"));
      break;
    case NewRemoteCode::dim:
      // Serial.print(F(" dim "));
      break;
  }
  if (receivedCode.dimLevelPresent) {
	Serial.print(F(" "));
    Serial.print(receivedCode.dimLevel);
  }
  Serial.println("");
  Serial.flush();
}



// --------------------------------------------------------------------------------
// LIVOLO
void showLivoloCode(livoloCode receivedCode) {
	
	Serial.print(F("< "));
	Serial.print(msgCnt);
	Serial.print(F(" 2 5 "));
	Serial.print(receivedCode.address);
	Serial.print(F(" "));
	Serial.print(receivedCode.unit);
	Serial.print(F(" "));
	Serial.print(receivedCode.level);

	
	if (debug >= 1) {
		Serial.print(F(" ! Livolo ")); 
#if STATISTICS==1
		Serial.print(F(", P1: ")); Serial.print(receivedCode.min1Period);
		Serial.print(F("-")); Serial.print(receivedCode.max1Period);
		Serial.print(F(", P3: ")); Serial.print(receivedCode.min3Period);
		Serial.print(F("-")); Serial.print(receivedCode.max3Period);
#endif
	}
	Serial.println("");
	msgCnt++;
}

// --------------------------------------------------------------------------------
// KOPOU
void showKopouCode(kopouCode receivedCode) {
	Serial.print(F("< "));
	Serial.print(msgCnt);
	Serial.print(F(" 2 6 "));
	Serial.print(receivedCode.address);
	Serial.print(F(" "));
	Serial.print(receivedCode.unit);
	Serial.print(F(" "));
	Serial.print(receivedCode.level);
	
	if (debug >= 1) {
		Serial.print(F(" ! Kopou ")); 
#if STATISTICS==1
		Serial.print(F(", P: ")); Serial.print(receivedCode.period);
		Serial.print(F(", P1: ")); Serial.print(receivedCode.minPeriod);
		Serial.print(F("-")); Serial.print(receivedCode.maxPeriod);
#endif
	}
	Serial.println("");
	msgCnt++;
}

// --------------------------------------------------------------------------------
// REMOTE
// General receiver for old Kaku, Impulse, etc.
// receivedCode is an unsigned gong with address and unit info
// At this moment the code is tuned for Action remotes.
// Must be recognizing the correct remote later (based on period?)
//
void showRemoteCode(unsigned long receivedCode, unsigned int period) {
	int i;
	byte level = 0;
	byte unit = 0;
	short address = 0;
	byte codec = 1;

	// Unfortunately code is Base3
	unsigned long code = receivedCode;

	// Action codec
	if ( (period > 120 ) && (period < 200 ) ) {
		for (i=0; i<2; i++) {
			level = level * 10;
			level += code % 3;
			code = code / 3;
		}
		// two bits, either 02 or 20, the 0 determines the value
		if (level == 20) { level = 0; }
		else { level = 1; }
		// 5 bits, 5 units. The position of the 0 determines the unit (0 to 5)	
		for (i =4; i >= 0; i--) {
			if ((code % 3) == 0) unit= i;
			code =  code / 3;
		}
		// Position of 1 bit determines the values
		for (i=0; i < 5; i++) {
			if ((code % 3) == 1) address += ( B00001<<i );
			code =  code / 3;
		}
	}
	// Following code should be equal for ALL receivers of this type
	Serial.print(F("< "));
	Serial.print(msgCnt);
	Serial.print(F(" 2 "));					// action is type 1, blokker type 2, old=3, Elro=4
	Serial.print(codec);
	Serial.print(F(" "));
	Serial.print(address);
	Serial.print(F(" "));
	Serial.print(unit);
	Serial.print(F(" "));
	Serial.print(level);

	if (debug >= 1) {
		Serial.print(F(" ! Remote ")); 
#if STATISTICS==1
		Serial.print(F(", P: ")); Serial.print(period);
#endif	 
	}
	Serial.println("");
	msgCnt++;
}

