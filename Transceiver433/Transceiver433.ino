/*
* Code for RF remote switch Gateway. 
* Focus on Kaku, wt440, Livolo devices first, more to follow
*
* Version 1.2 beta; 150811
* (c) M. Westenberg (mw12554@hotmail.com)
*
* Based on work of many:
* 	Randy Simons (Kaku and InterruptChain) 
* 	and others (Wt440) etc.
*
* Connect the receiver to digital pin 2, sender no pin 8.
*/
#define CODECS 7

// Devices
#define KAKU    0
#define ACTION  1
#define BLOKKER 2
#define KAKUOLD 3
#define ELRO    4
#define LIVOLO  5 
#define KOPOU   6

// Sensors
#define WT440  16
#define OREGON 17

// Transmitters
#include <LamPITransmitter.h>
#include <RemoteTransmitter.h>

// Receivers
#include <wt440Receiver.h>
#include <kakuReceiver.h>
#include <livoloReceiver.h>
#include <kopouReceiver.h>
#include <RemoteReceiver.h>

// Others
#include <InterruptChain.h>

int debug;
int  readCnt;				// Character count in buffer
char readChar;				// Last character read from tty
char readLine[64];			// Buffer of characters read
int msgCnt;
unsigned long codecs;		// must be at least 32 bits for 32 positions.

// Create a transmitter using digital pin 8 to transmit,
// with a period duration of 260ms (default), repeating the transmitted
// code 2^3=8 times.
NewRemoteTransmitter transmitter(8, 260, 3);
ActionTransmitter atransmitter(8, 195, 3);

// --------------------------------------------------------------------------------
//
void setup() {
  // As fast as possible for USB bus
  Serial.begin(115200);
  msgCnt = 0;
  readCnt = 0;
  debug = 0;
  codecs = 0;

  // Initialize receiver on interrupt 0 (= digital pin 2), calls the callback (for example "showKakuCode")
  // after 2 identical codes have been received in a row. (thus, keep the button pressed for a moment)

  wt440Receiver::init(-1, 1, showWt440Code);
  NewRemoteReceiver::init(-1, 2, showKakuCode);
  livoloReceiver::init(-1, 2, showLivoloCode);
  kopouReceiver::init(-1, 2, showKopouCode);
  RemoteReceiver::init(-1, 3, showRemoteCode);

  // Change interrupt mode to CHANGE (on flanks)
  InterruptChain::setMode(0, CHANGE);
  
  // Define the interrupt chain
  // The sequence might be relevant, put short pulse protocols first (Such as Livolo)
  InterruptChain::addInterruptCallback(0, wt440Receiver::interruptHandler); onCodec(WT440);
  InterruptChain::addInterruptCallback(0, livoloReceiver::interruptHandler); onCodec(LIVOLO);
  InterruptChain::addInterruptCallback(0, NewRemoteReceiver::interruptHandler); onCodec(KAKU);
  InterruptChain::addInterruptCallback(0, kopouReceiver::interruptHandler);	onCodec(KOPOU);
  InterruptChain::addInterruptCallback(0, RemoteReceiver::interruptHandler); onCodec(ACTION);
}

// --------------------------------------------------------------------------------
//
void loop() {
  char * pch;
  InterruptChain::disable(0);					// Set interrupts off  
  
  while (Serial.available()) {
	readChar = Serial.read();					// Read the requested byte from serial
	if (readChar == '\n') {						// If there is a newLine in the input
	  readLine[readCnt]='\0';	  				// Overwrite the \n char, close the string
	  Serial.print("! "); Serial.print(readCnt);
	  Serial.print(": \{"); Serial.print(readLine); Serial.println("\}"); Serial.flush();
	  
	  parseCmd(readLine);						// ACTION: Parsing the readLine for actions

	  readLine[0]='\0';
	  readCnt=0;  
	}
	else {
	  readLine[readCnt]=readChar;
	  readCnt++;
	  readLine[readCnt]='\0';
	}
  }//available
  InterruptChain::enable(0);					// Set interrupts on again
}

// ***************************** GENERIC ******************************************
void onCodec (char codec) {
	codecs = codecs | ( 1 << codec	);	// AND
}

void offCodec (char codec) {
  codecs = codecs & ~( 1 << codec );		// NAND, no effect for 0, but for 1 bit makes 0
}

void setCodec (char codec, char val) {
  codecs = codecs & ~( 1 << codec );		// NAND, no effect for 0, but for 1 bit makes 0
}

// ************************* TRANSMITTER PART *************************************

// --------------------------------------------------------------------------------
// PARSE COMMAND (first part of message)
//
void parseCmd(char *readLine) 
{
  int cnt, cmd, codec, ret;
  char *pch;					// Pointer to character position
  if (readLine[0] != '>') {
	Serial.println("! Error: cmd starts with \">\" ");
  } else { 
	pch = readLine+1;
  }
  
  // 
  if (debug >= 1) {
	Serial.print("!< ");
	Serial.print(msgCnt);
	Serial.print(" ");
	Serial.print(cmd);
	Serial.print(" ");
	Serial.print(codec);
  }
  
  pch = strtok (pch, " ,."); cnt = atoi(pch);
  pch = strtok (NULL, " ,."); cmd = atoi(pch);
  
  switch(cmd) {
    case 0:								// admin
		parseAdmin(pch);
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
			default:
				Serial.println("! ERROR: Unknown codec");
				return;
		}
		msgCnt++;
	break;
	case 2:								// 
		//
	break;
	default:
		Serial.print("! Unknown command ");
		Serial.println(cmd);
  }
  msgCnt++;
}

// --------------------------------------------------------------------------------
// Do parsing of Admin command
// print back to caller the received code
//
void parseAdmin(char *pch)
{
	int cmd;
	int group;
	int codec;
	int val;
	
	pch = strtok (NULL, " ,."); cmd = atoi(pch);
	switch (cmd) {
		case 0:						// List device Codecs
			Serial.print("! Cod = "); 
			Serial.println(codecs,BIN);
		break;
		case 1:						// List sensor Codecs
			pch = strtok (NULL, " ,."); codec = atoi(pch);
			pch = strtok (NULL, " ,."); val = atoi(pch);
			if (val == 1) onCodec(codec); else offCodec(codec);
			Serial.print("! set Cod = "); Serial.println(codecs,BIN);
		break;
		case 2:						// Ask for statistics
			Serial.print("! Stat = "); Serial.println("");
		break;
		case 3:						// Debug level
			pch = strtok (NULL, " ,."); debug = atoi(pch);
			if (debug > 2) { debug = 1; }
			if (debug < 0) { debug = 0; }
			Serial.print("! Debug = "); Serial.println(debug);
		break;
		default:
			Serial.println("! ERROR unknown admin cmd");
	}

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
    Serial.println("! ERROR Invalid input. Dim level must be between 0 and 15!");
  }
  
  if (debug >= 1){
	Serial.print(" "); Serial.print(group);
	Serial.print(" "); Serial.print(unit);
	Serial.print(" "); Serial.println(level);
	Serial.flush();
  }
}

// --------------------------------------------------------------------------------
// Do parsing of Action specific command
// print back to caller the received code
//
void parseAction(char *pch)
{
	byte group;
	char unit, level;
	boolean lvl = false;
	pch = strtok (NULL, " ,."); group = atoi(pch);
	pch = strtok (NULL, " ,."); unit = atoi(pch);  
	pch = strtok (NULL, " ,."); level = atoi(pch);
	if (level == 1) lvl = true;
	
	Serial.print("! Action :: group: ");
	Serial.print(group);
	Serial.print(", unit: ");
	Serial.print(unit);
	Serial.print(", lvl: ");
	Serial.println(lvl);

	atransmitter.sendSignal(group, unit, lvl);

}



// ************************* RECEIVER STUFF BELOW *********************************

// --------------------------------------------------------------------------------
// KAKU
// Callback function is called only when a valid code is received.
//
void showKakuCode(NewRemoteCode receivedCode) {
  // Note: interrupts are disabled. You can re-enable them if needed.

  // Print the received code. 2 for received codes and 0 for codec of Kaku
  Serial.print("< ");
  Serial.print(msgCnt);
  Serial.print(" 2 0 ");
  Serial.print(receivedCode.address);
  msgCnt++;
  
  if (receivedCode.groupBit) {
    Serial.print(" group ");
  } 
  else {
    Serial.print(" ");
    Serial.print(receivedCode.unit);
  }

  switch (receivedCode.switchType) {
    case NewRemoteCode::off:
      Serial.print(" off");
      break;
    case NewRemoteCode::on:
      Serial.print(" on");
      break;
    case NewRemoteCode::dim:
      // Serial.print(" dim ");
      break;
  }

  if (receivedCode.dimLevelPresent) {
	Serial.print(" ");
    Serial.print(receivedCode.dimLevel);
  }
  Serial.println("");
  Serial.flush();
}

// --------------------------------------------------------------------------------
// WT440
void showWt440Code(wt440Code receivedCode) {
	
	Serial.print("< ");
	Serial.print(msgCnt);
	Serial.print(" 3 0 ");
	Serial.print(receivedCode.address);
	Serial.print(" ");
	Serial.print(receivedCode.channel);
	Serial.print(" ");
	Serial.print(receivedCode.temperature);
	Serial.print(" ");
	Serial.print(receivedCode.humidity);
	Serial.println("");
	
	if (debug >= 1) {
		Serial.print("! WT440 code:: addr: "); Serial.print(receivedCode.address);
		Serial.print(", Chl: "); Serial.print(receivedCode.channel);
		Serial.print(", Wconst: "); Serial.print(receivedCode.wconst);
		Serial.print(", Humid: "); Serial.print(receivedCode.humidity);
		Serial.print(", Temp: "); Serial.print(receivedCode.temperature);
		Serial.print(", Par: "); Serial.print(receivedCode.par);
		Serial.println(""); 
		Serial.flush();
	}
	msgCnt++;
}

// --------------------------------------------------------------------------------
// LIVOLO
void showLivoloCode(livoloCode receivedCode) {
	
	Serial.print("< ");
	Serial.print(msgCnt);
	Serial.print(" 2 5 ");
	Serial.print(receivedCode.address);
	Serial.print(" ");
	Serial.print(receivedCode.unit);
	Serial.print(" ");
	Serial.print(receivedCode.level);
	Serial.println("");
	
	if (debug >= 1) {
		Serial.print("! Livolo:: addr: "); Serial.print(receivedCode.address);
		Serial.print(", Unit: "); Serial.print(receivedCode.unit);
		Serial.print(", Level: "); Serial.print(receivedCode.level);
		Serial.println(""); Serial.flush();
	}
	msgCnt++;
}

// --------------------------------------------------------------------------------
// KOPOU
void showKopouCode(kopouCode receivedCode) {
	
	Serial.print("< ");
	Serial.print(msgCnt);
	Serial.print(" 2 6 ");
	Serial.print(receivedCode.address);
	Serial.print(" ");
	Serial.print(receivedCode.unit);
	Serial.print(" ");
	Serial.print(receivedCode.level);
	Serial.println("");
	
	if (debug >= 1) {
		Serial.print("! Kopou code:: addr: "); Serial.print(receivedCode.address);
		Serial.print(", Unit: "); Serial.print(receivedCode.unit);
		Serial.print(", Level: "); Serial.print(receivedCode.level);
		Serial.print(", Per: "); Serial.print(receivedCode.period);
		Serial.print(", Min: "); Serial.print(receivedCode.minPeriod);
		Serial.print(", Max: "); Serial.print(receivedCode.maxPeriod);
		Serial.println(""); 
		Serial.flush();
	}
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
	// Following code hould be equal for ALL receivers of this type
	Serial.print("< ");
	Serial.print(msgCnt);
	Serial.print(" 2 ");					// action is type 1, blokker type 2, old=3, Elro=4
	Serial.print(codec);
	Serial.print(" ");
	Serial.print(address);
	Serial.print(" ");
	Serial.print(unit);
	Serial.print(" ");
	Serial.println(level);

	if (debug >= 1) {
		Serial.print("! Remote code:: "); Serial.print(codec);
		Serial.print(", Addr: "); Serial.print(address);
		Serial.print(", Unit: "); Serial.print(unit);
		Serial.print(", Level: "); Serial.print(level);
		Serial.print(", Period: "); Serial.print(period);
		Serial.println(""); 
		Serial.flush();
	}
	msgCnt++;
}

