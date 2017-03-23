/* Arduino code to validate & decode Lazertag (LTTO Compatible) beacon tag and data signatures

History:
Rev A.1  Sherpadoug 5/18/2013
Rev A.2  Heavily modified by Izzy, 2013/05/18.
Rev A.3  Added functionality to count Health and Display by D.Westaby 12/26/2013

Original Forum Post: https://groups.yahoo.com/neo/groups/Lasertag_Design/conversations/topics/380

Usage:
    Setup unhosted game.
    - Press shield button to swap between 10 and 25 health start.
    - Display will read 10 or 25 health.
    - Press fire to start the game.
    Play game.
    - As you take tags, display will update with health remaining.
    - Turn gun off/on to reset the counter.

Other Notes:

   7-Segment Display Layout

            1A            2A
          ------        ------
         |      |      |      |
       1F|      |1B  2F|      |2B
         |  1G  |      |  2G  |
          ------        ------
         |      |      |      |
       1E|      |1C  2E|      |2C
         |  1D  |      |  2D  |
          ------  .     ------  .

   Pinout Mapping

   Arduino  I/O  Signal Type      Name
   -------------------------------------
      0      O   Display Output   2C
      1      O   Display Output   2G
      2      O   Display Output   2D
      3      O   Display Output   2E
      4      O   Display Output   1C
      5      O   Display Output   1D
      6      O   Display Output   1E
      7      I   Switch Input     Tagger's Trigger Button
      8      O   Display Output   1F
      9      O   Display Output   1G
      10     O   Display Output   1A
      11     O   Display Output   1B
      12     I   Signal Input     IR Reciever Input
      13     I   Switch Input     Tagger's Shield Button
      14     O   Display Output   2F
      15     O   Display Output   2A
      16     O   Display Output   2B

*/


/* ------------------------------------
            Display Constants
   ------------------------------------ */

#define ON  (HIGH) /* Common Ground Display, HIGH is ON */
#define OFF (LOW)

/* ------------------------------------
         Display Look Up Tables
   ------------------------------------ */
byte pinMap_digit1[7] =
{
   10,   // 1A
   11,   // 1B
   4,    // 1C
   5,    // 1D
   6,    // 1E
   8,    // 1F
   9,    // 1G
};
byte pinMap_digit2[7] =
{
   A2,   // 2A
   A3,   // 2B
   A4,    // 2C
   2,    // 2D
   3,    // 2E
   A0,   // 2F
   A5     // 2G
};

/* ------------------------------------
             PIN Assignments
   ------------------------------------ */
/* Signal Name      (Board ID) */
const int sensorPin  = 12;     /* IR receiver pin  (FX) */
const int fire_Pin   = 7;      /* Fire input pin   (FIRE) */
const int shield_Pin = 13;     /* Shield input pin (LED) */

/* ------------------------------------
         Tag Reading Structures
   ------------------------------------ */
struct tag
{
   unsigned long data;       /* We'll only use 9 bits of this, but there's no 9-bit data type. */
   unsigned char counter;    /* For counting where you are in receiving */
   unsigned char databits;   /* How many data bits there are. */
   unsigned char headertype; /* Whether this is a 3/6/6 or a 3/6/3 header. */
};

/*  databits definition
   00    5 bits
   01    7 bits
   10    8 bits
   11    9 bits

    headertype definition
   0     3/6/3
   1     3/6/6
*/

/* ------------------------------------
          Tag Reading Constants
   ------------------------------------ */
unsigned long syncTimeout = 10000;  /* Sync timeout for pulseIn function 10ms */
unsigned long bitTimeout  =  5000;  /* Bit timeout for pulseIn function 5ms */

const int startBitMax = 3500;       /* IR sync length limits */
const int startBitMin = 2500;

/* ------------------------------------
            Global Variables
   ------------------------------------ */
unsigned long pulse;
unsigned long bitEdge;
unsigned char team;
unsigned char player;
unsigned char mega;
unsigned char health = 10;

boolean captureSuccessful = false;
boolean gamestarted = false;

tag myTag;

/* ------------------------------------
              Main Functions
   ------------------------------------ */
void setup()
{
   /*  Initialization */

   int segment;
   int count;

   /* Set Pin Directions */
   for (segment = 0; segment < 7; segment++)
   {
      pinMode( pinMap_digit1[segment], OUTPUT);
      pinMode( pinMap_digit2[segment], OUTPUT);
   }

   pinMode(sensorPin,  INPUT);
   pinMode(fire_Pin,   INPUT);
   pinMode(shield_Pin, INPUT);

//while (1)
//{
//  digitalWrite(10,digitalRead(shield_Pin));
//}

   Serial.begin(9600);
   Serial.println("LTTO Display");

   /* Information Gathering */
   get_game_info();

Serial.println("Starting Game Count Down");

   /* Wait for Game Start */
   /* Fire button was pressed, game is about to start */
//DEBUG, should be 9 or 10 count
   for (count = 1; count >= 0; count--)
   {
      /* Display 10 Second Countdown */
      delay(1000);
      display_num( count );
Serial.println(count, DEC);
   }

Serial.println("Game Started !");
      display_num( health );
}

void loop()
{

   pulse = pulseIn(sensorPin, LOW);
   if(pulse > startBitMin && pulse < startBitMax)
   {

      //Saw a ~3ms pulse. Probably a start bit. Let's find out!
      pulse = pulseIn(sensorPin, LOW);
      if(pulse > (2 * startBitMin) && pulse < (2 * startBitMax))
      {
         //~6ms
         bitEdge = pulse / 4; //Roughly 1.5ms, corrected for clock errors.
         pulse = pulseIn(sensorPin, LOW);
         if(pulse > startBitMin && pulse < startBitMax)
         {

            //~3ms. We have a 3/6/3 header!
            myTag.headertype = 0;
            GetIR();
         }
         else if(pulse > (2 * startBitMin) && pulse < (2 * startBitMax))
         {
            //~6ms. We have a 3/6/6 header!
            myTag.headertype = 1;
            GetIR();
         }
         else
         {
            //Just noise. How disappointing.
Serial.println(pulse, DEC);
         }
      }

   }


   if(captureSuccessful == true)
   {
      //We caught something! Let's print it out!
      print_tag();
   }
}

/* ------------------------------------
              Sub Functions
   ------------------------------------ */
void GetIR()
{
   unsigned char incompleteData = 1;
   unsigned long tempData;
   myTag.counter = 0;

   while(incompleteData)
   {
      pulse = pulseIn(sensorPin, LOW, 5000);
      if(pulse == 0)
      {
         // pulseIn() timed out. Capture complete.
         incompleteData = 0;
      }
      else
      {
         tempData = (unsigned long)((pulse > bitEdge) ? 1 : 0) << (8 - myTag.counter);
         // Get whether the bit captured was a 1 or a 0, then shift it over
         // to be in the correct place for how many bits we've captured.
         myTag.data |= tempData;
         // Store the bit into the overall captured data.
         myTag.counter++;
      }
   }

   /* Shift things to the right to account for how many data bits we actually
      captured. This also doubles as a basic check as to whether it was a
      valid capture. We will only ever capture 5, 7, 8 or 9 bits for any given packet. */
   switch(myTag.counter)
   {
      case 5:
         myTag.data = (myTag.data >> 4);
         myTag.databits = 0;
         captureSuccessful = true;
         break;
      case 7:
         myTag.data = (myTag.data >> 2);
         myTag.databits = 1;
         captureSuccessful = true;
         break;
      case 8:
         myTag.data = (myTag.data >> 1);
         myTag.databits = 2;
         captureSuccessful = true;
         break;
      case 9:
         captureSuccessful = true;
         myTag.databits = 3;
         break;
      default:
         //We got noise.
         captureSuccessful = false;
   }
}


void print_tag()
{
   captureSuccessful = false; // Clear the flag, so we don't print this one out again.
   if(myTag.headertype)
   {
      // 3/6/6 header
      switch(myTag.databits)
      {
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
            if(myTag.databits == 0)
            {
               Serial.println("5");
            }
            else
            {
               Serial.println(myTag.databits + 0x36);
               //Bit of a hack to get the number of bits out of it.
            }
            Serial.println(myTag.data, BIN);
            break;
      }
   }
   else
   {
      // 3/6/3 header
      switch(myTag.databits)
      {
         case 1:
            // Shot.
            team = (myTag.data >> 5) & 0x03;
            player = (myTag.data >> 2) & 0x07;
            mega = myTag.data & 0x03;

            Serial.println("S");
            Serial.println(team);
            Serial.println(player);
            Serial.println(mega);

            /* Shot Received, Update Health Display */
            process_tag(mega);
            break;
         case 2:
            // Data byte.
            Serial.println("D");
            Serial.println((unsigned char)(myTag.data & 0xFF), BIN);
            break;
         case 3:
            // Multi-byte start or checksum.
            if(myTag.data & 0x0100)
            {
               //Multi-byte start.
               Serial.println("M");
               Serial.println((unsigned char)(myTag.data & 0xFF), BIN);
            }
            else
            {
               //Checksum.
               Serial.println("C");
               Serial.println((unsigned char)(myTag.data & 0xFF), BIN);
            }
            break;
         default:
            // Unknown. Shouldn't happen.
            Serial.println("?");
            Serial.println("3");
            if(myTag.databits == 0)
            {
               Serial.println("5");
            }
            else
            {
               Serial.println(myTag.databits + 0x36);
               //Bit of a hack to get the number of bits out of it.
            }
            Serial.println(myTag.data, BIN);
            break;
      }
   }
}
//--------------------------------------
//          Button Subroutines         |
//--------------------------------------
int button_is_pressed(int button)
{
   /* Check if Button was Pressed for at least 1ms */
   if (!digitalRead(button))
   {
      delay(1);
      if (!digitalRead(button))
      {
         return true;
      }
   }
   return false;
}

//--------------------------------------
//          Health Subroutines         |
//--------------------------------------

void get_game_info()
{

   /* Update display with selected health */
   display_num( health );

   /* Wait for Fire Button Press */
   while (!button_is_pressed(fire_Pin))
   {
     /* Wait for sheild presses (ISR) */

        if(button_is_pressed(shield_Pin))
        {
          if (health == 25)
          {
            health = 10;
          }
          else
          {
            health = 25;
          }

            /* Update display with selected health */
            display_num( health );
            delay(200);
          }
      }
    Serial.println("Got Game Info");
    Serial.println("Health = ");
    Serial.println(health, DEC);
}

void process_tag(unsigned char tag_num)
{
   /* Convert number of megas to number of tags */
   tag_num++;

   /* Subtract number of tags from health remaining */
   if (health >= tag_num)
   {
      health -= tag_num;
   }
   else
   {
      /* Not enough health remains, lock at zero */
      health = 0;
   }

   /* Update display with remaining health */
   display_num( health );
}

//--------------------------------------
//           Math Subroutine           |
//--------------------------------------

/* Author: robtillaart */
int dec2bcd(int dec)
{
   return (dec / 10) * 16 + (dec % 10);
}

//--------------------------------------
//         Display Subroutine          |
//--------------------------------------
void display_num(int disp_output)
{
   /* Look Up Table */
   byte seg_array[16] =
   {
      // Segments  Hex Number
      // GFEDCBA
      0b00111111, // 0  126
      0b00000110, // 1  6
      0b01011011, // 2  91
      0b01001111, // 3  79
      0b01100110, // 4  102
      0b01101101, // 5  109
      0b01111101, // 6  125
      0b00000111, // 7  7
      0b01111111, // 8  127
      0b01100111, // 9  103
      0b01110111, // A  119
      0b01111100, // B  124
      0b00111001, // C  57
      0b01011110, // D  94
      0b01111001, // E  121
      0b01110001  // F  113
      //0b01000000  // -  (This entry is impossible for BCD)
   };

   /* Variables */
   int segment;
   int digit1;
   int digit2;

   /* ====================================
                 BCD Conversion
      ==================================== */

   /* Decimal Numbers need to be in BCD form */
   disp_output = dec2bcd(disp_output);

   digit1 = (disp_output >> 4 ) & 0x0F;;
   digit2 = disp_output & 0x0F;

   /* ====================================
             Assemble Ouput Digits
      ==================================== */

   for (int i = 0; i < 7; i = i + 1)
   {
      digitalWrite( pinMap_digit1[i], (seg_array[digit1] >> i) & 1);
      digitalWrite( pinMap_digit2[i], (seg_array[digit2] >> i) & 1);
   }

} /* End Display Num Function */
