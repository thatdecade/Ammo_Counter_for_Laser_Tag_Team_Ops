//Arduino code to validate & decode Lazertag beacon tag and data signatures
//Rev A.1 Sherpadoug 5/18/2013
// Heavily modified by Izzy, 2013/05/18.

struct tag
{
	unsigned long data;
		// We'll only use 9 bits of this, but there's no 9-bit data type.
	unsigned char counter;
		// For counting where you are in transmitting/receiving
	unsigned char databits;
		//How many data bits there are.
		/*
			00		5 bits
			01		7 bits
			10		8 bits
			11		9 bits
		*/
	unsigned char headertype;
		// Whether this is a 3/6/6 or a 3/6/3 header.
		/*
			0		3/6/3
			1		3/6/6
		*/
};

const int sensorPin = 2;            //IR receiver pin

unsigned long syncTimeout = 10000;  //sync timeout for pulseIn function 10ms
unsigned long bitTimeout = 5000;    //bit timeout for pulseIn function 5ms

const int startBitMax = 3500;       //IR sync length limits
const int startBitMin = 2500;

unsigned long pulse;
unsigned long bitEdge;
unsigned char captureSuccessful;
unsigned char team;
unsigned char player;
unsigned char mega;
tag myTag;

void GetIR() {
	unsigned char incompleteData = 1;
	unsigned long tempData;
	myTag.counter = 0;
	
	while(incompleteData) {
		/*
		Doing things a bit different here. pulseIn() has a timeout feature.
		We know that the longest data bit in the LTTO protocol is 2ms, as is the longest delay
			between them, adding up to 4ms.
		If we go longer than 5ms without receiving a data bit, we can be fairly certain that
			we're not going to be receiving another for this packet.
		So, we set the pulseIn() timeout to 5000, and if pulse is equal to 0 after the call
			to pulseIn(), we timed out, so we'll set incompleteData to 0 and break out of the loop.
		*/
		
		pulse = pulseIn(sensorPin, LOW, 5000);
		if(pulse == 0) {
			// pulseIn() timed out. Capture complete.
			incompleteData = 0;
		} else {
			tempData = (unsigned long)((pulse > bitEdge)? 1 : 0) << (8 - myTag.counter);
				// Get whether the bit captured was a 1 or a 0, then shift it over to be in the
				// correct place for how many bits we've captured.
			myTag.data |= tempData;
				// Store the bit into the overall captured data.
			myTag.counter++;
		}
	}
	
	// Now we need to shift things to the right to account for how many data bits we actually
	// captured.
	// This also doubles as a basic check as to whether it was a valid capture.
	// We will only ever capture 5, 7, 8 or 9 bits for any given packet.
	switch(myTag.counter) {
		case 5:
			myTag.data = (myTag.data >> 4);
			myTag.databits = 0;
			captureSuccessful = 1;
			break;
		case 7:
			myTag.data = (myTag.data >> 2);
			myTag.databits = 1;
			captureSuccessful = 1;
			break;
		case 8:
			myTag.data = (myTag.data >> 1);
			myTag.databits = 2;
			captureSuccessful = 1;
			break;
		case 9:
			captureSuccessful = 1;
			myTag.databits = 3;
			break;
		default:
			//We got noise.
			captureSuccessful = 0;
	}
}

void setup() {
  pinMode(sensorPin, INPUT);   //This sets the IR Sensor pin as an INPUT
  Serial.begin(9600);
  Serial.println("LTTO Explorer Rev 1-izzy");
}

void loop() {
	pulse = pulseIn(sensorPin, LOW);
	if(pulse > startBitMin && pulse < startBitMax) {
		//Saw a ~3ms pulse. Probably a start bit. Let's find out!
		pulse = pulseIn(sensorPin, LOW);
		if(pulse > (2 * startBitMin) && pulse < (2 * startBitMax)) {
			//~6ms
			bitEdge = pulse / 4; //Roughly 1.5ms, corrected for clock errors.
			pulse = pulseIn(sensorPin, LOW);
			if(pulse > startBitMin && pulse < startBitMax) {
				//~3ms. We have a 3/6/3 header!
				myTag.headertype = 0;
				GetIR();
			} else if(pulse > (2 * startBitMin) && pulse < (2 * startBitMax)) {
				//~6ms. We have a 3/6/6 header!
				myTag.headertype = 1;
				GetIR();
			} else {
				//Just noise. How disappointing.
			}
		}
	}
	
	if(captureSuccessful) {
		//We caught something! Let's print it out!
		captureSuccessful = 0; // Clear the flag, so we don't print this one out again.
		if(myTag.headertype) {
			// 3/6/6 header
			switch(myTag.databits) {
				case 0:
					// LTTO beacon.
					team = (myTag.data >> 3) & 0x03;
					mega = myTag.data & 0x07; //Assorted bits. Not actually Mega.
					
					Serial.println("B");
					Serial.println(team);
					Serial.println(mega, BIN);
					break;
				case 3:
					// LTAR beacon.
					mega = (myTag.data >> 5) & 0x1F; //Assorted bits. Not actually Mega.
					team = (myTag.data >> 3) & 0x03;
					player = myTag.data & 0x07;
					
					Serial.println("L");
					Serial.println(mega, BIN);
					Serial.println(team);
					Serial.println(player);
					break;
				default:
					// Unknown. Shouldn't happen.
					Serial.println("?");
					Serial.println("6");
					if(myTag.databits == 0) {
						Serial.println("5");
					} else {
						Serial.println(myTag.databits + 0x36);
						//Bit of a hack to get the number of bits out of it.
					}
					Serial.println(myTag.data, BIN);
					break;
			}
		} else {
			// 3/6/3 header
			switch(myTag.databits) {
				case 1:
					// Shot.
					team = (myTag.data >> 5) & 0x03;
					player = (myTag.data >> 2) & 0x07;
					mega = myTag.data & 0x03;
					
					Serial.println("S");
					Serial.println(team);
					Serial.println(player);
					Serial.println(mega);
					break;
				case 2:
					// Data byte.
					Serial.println("D");
					Serial.println((unsigned char)(myTag.data & 0xFF), BIN);
					break;
				case 3:
					// Multi-byte start or checksum.
					if(myTag.data & 0x0100) {
						//Multi-byte start.
						Serial.println("M");
						Serial.println((unsigned char)(myTag.data & 0xFF), BIN);
					} else {
						//Checksum.
						Serial.println("C");
						Serial.println((unsigned char)(myTag.data & 0xFF), BIN);
					}
					break;
				default:
					// Unknown. Shouldn't happen.
					Serial.println("?");
					Serial.println("3");
					if(myTag.databits == 0) {
						Serial.println("5");
					} else {
						Serial.println(myTag.databits + 0x36);
						//Bit of a hack to get the number of bits out of it.
					}
					Serial.println(myTag.data, BIN);
					break;
			}
		}
	}
}