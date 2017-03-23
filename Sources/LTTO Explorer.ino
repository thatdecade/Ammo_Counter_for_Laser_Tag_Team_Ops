//Arduino code to validate & decode Lazertag beacon tag and data signatures
//Rev A.1 Sherpadoug 5/18/2013

int tagMem[9];                      //Storage array for a tag signature
int tagType;                        //tagType is number of bits & sig type
						    //-1 for error
                                    //beacon sigs are 5 bits
                                    //tag sigs are 7 bits
			            	    //data sigs are 8 bits
						    //checksum & packet sigs are 9 bits
const int sensorPin = 2;            //IR receiver pin

unsigned long syncTimeout = 10000;  //sync timeout for pulseIn function 10ms
unsigned long bitTimeout = 5000;    //bit timeout for pulseIn function 5ms

const int startBitMax = 3500;       //IR sync length limits
const int startBitMin = 2500;

void GetIR() {                      //The start bit got us to this routine.
				    //Next is the sync bit
  int bitcount;                     //length of current bit
  int bitEdge;                      //0 vs 1 bit length threshold
  bitcount = pulseIn(sensorPin, LOW, syncTimeout);
  if(bitcount > (2 * startBitMin) & bitcount < (2 * startBitMax)){ //6ms more or less
    tagType = 5;                    //double length sync bit flags a 5 bit beacon sig
    bitEdge = bitcount/4;           //6ms/4 = 1.5ms corrected for clock errors
    for(int i=1; i<= tagType; i++){
      bitcount = pulseIn(sensorPin, LOW, bitTimeout);
      if (bitcount) {
        tagMem[i] = (bitcount > bitEdge)? 1 : 0;
      }
      else{                  //bitcount = 0 means pulseIn timed out
        tagType = -1;        //corrupt beacon sig
        return;
      }
    }
    return;
  }
  else if(bitcount > startBitMin & bitcount < startBitMax){  //3ms more or less
    bitEdge = bitcount/2;                       //3ms/2 = 1.5ms corrected for clock errors
    for(tagType = 0; tagType<=9; tagType++){    //need to count bits as they come in, 10 bit max
      bitcount = pulseIn(sensorPin, LOW, bitTimeout);
      if (bitcount) {
        tagMem[tagType] = (bitcount > bitEdge)? 1 : 0;
      }
      else{                   //bitcount = 0 means pulseIn timed out
        return;               //note last increment is kept so for bits 0 to 4 tagType = 5
      }
    }
  }
  else{        //sync was not either 6ms or 3ms
    tagType = 0 - bitcount;			//Big negative tagMem[0] means bad sync bit
    return;
  }
}

void setup() {
  pinMode(sensorPin, INPUT);   //This sets the IR Sensor pin as an INPUT
  Serial.begin(9600);
  Serial.println("LTTO Explorer Rev 1");
}

void loop() {
  int Team;
  int Player;
  int Mega;
  if (pulseIn(sensorPin, LOW) > startBitMin) {      //This checks for the start bit of a valid sig
    GetIR();                                      //Read the rest of the sig
    switch (tagType){
		case 5:						//5 bits = Beacon sig
			Serial.print("B");
			Team = tagMem[0] * 2 + tagMem[1];
			Player = tagMem[2];                     //"HF" bit whatever that is
			Mega = tagMem[3] * 2 + tagMem[4];       //two X bits
       			Serial.print(Team);
              		Serial.print(Player);
                    	Serial.print(Mega, BIN);
        		break;
		case 7:						//7 bits = Tag sig
			Serial.print("T");
                        Team = tagMem[0] * 2 + tagMem[1];
                        Player = (tagMem[2] * 2 + tagMem[3]) * 2 + tagMem[4];
                        Mega = tagMem[5] * 2 + tagMem[6];
                        Serial.print(Team);
             		Serial.print(Player);
                    	Serial.print(Mega);
			break;
		case 8:                                        //8 bits = Data sig
                        Serial.print("D");
                        Mega = 0;
                        for(int i=0; i<8; i++)
                          Mega = (Mega * 2)+ tagMem[i];
      		       	Serial.print(Mega, HEX);
			break;
		case 9:
                        if (tagMem[0] = 1){                    //Checksum sig
                           Serial.print("C");
                           Mega = 0;
                           for(int i=0; i<8; i++)
                           Mega = (Mega * 2)+ tagMem[i];
      		       	   Serial.print(Mega, HEX);
	                   break;
                        }
		        else{                                  //Packet sig
                           Serial.print("P");
                           Mega = 0;
                           for(int i=0; i<8; i++)
                           Mega = (Mega * 2)+ tagMem[i];
      		       	   Serial.print(Mega, HEX);
	                   break;
                        }
                default:						//other valid sig
			Serial.print("X");
      		        for (int i = 0; i <= 9; i++){	//Dumps collected data (10 numbers)
                                Serial.print(" ");
        			Serial.print(tagMem[i]);
			}
	 } // close switch
         Serial.println();
  } // close If (PulseIn...
}
