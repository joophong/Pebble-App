/***************************************************************************

                     Copyright 2008 Gravitech
                        All Rights Reserved

****************************************************************************/

/***************************************************************************
 File Name: I2C_7SEG_Temperature.pde

 Hardware: Arduino Diecimila with 7-SEG Shield

 Description:
   This program reads I2C data from digital thermometer and display it on 7-Segment

 Change History:
   03 February 2008, Gravitech - Created

****************************************************************************/

#include <Wire.h> 
 
#define BAUD (9600)    /* Serial baud define */
#define _7SEG (0x38)   /* I2C address for 7-Segment */
#define THERM (0x49)   /* I2C address for digital thermometer */
#define EEP (0x50)     /* I2C address for EEPROM */
#define RED (3)        /* Red color pin of RGB LED */
#define GREEN (5)      /* Green color pin of RGB LED */
#define BLUE (6)       /* Blue color pin of RGB LED */

#define COLD (23)      /* Cold temperature, drive blue LED (23c) */
#define HOT (26)       /* Hot temperature, drive red LED (27c) */

const byte NumberLookup[16] =   {0x3F,0x06,0x5B,0x4F,0x66,
                                 0x6D,0x7D,0x07,0x7F,0x6F, 
                                 0x77,0x7C,0x39,0x5E,0x79,0x71};

/* Function prototypes */
void Cal_temp (int&, byte&, byte&, bool&);
void Dis_7SEG (int, byte, byte, bool, bool);
void Send7SEG (byte, byte, bool);
void SerialMonitorPrint (byte, int, bool, bool);
void UpdateRGB (byte);
void Re_Cal(int& , byte& , bool& );
void Dis_7SEG_Time(int);
bool Time = 0;
bool Report = 1;

/***************************************************************************
 Function Name: setup

 Purpose: 
   Initialize hardwares.
****************************************************************************/

void setup() 
{ 
  Serial.begin(BAUD);
  Wire.begin();        /* Join I2C bus */
  pinMode(RED, OUTPUT);    
  pinMode(GREEN, OUTPUT);  
  pinMode(BLUE, OUTPUT);   
  delay(500);          /* Allow system to stabilize */
} 

/***************************************************************************
 Function Name: loop

 Purpose: 
   Run-time forever loop.
****************************************************************************/
 
void loop() 
{ 
  int Decimal;
  byte Temperature_H, Temperature_L, counter, counter2;
  bool IsPositive;
  bool IsFer = 0;
  int input = 0;
  int time_loop = 0;
  char time_string[5];
  strcpy(time_string,"");
  char time_char = 'a';
  int time_value;
  int RED_READ = 0;
  int GREEN_READ = 0;
  int BLUE_READ = 0;
  int count = 0;
  
  /* Configure 7-Segment to 12mA segment output current, Dynamic mode, 
     and Digits 1, 2, 3 AND 4 are NOT blanked */
     
  Wire.beginTransmission(_7SEG);   
  byte val = 0; 
  Wire.write(val);
  val = B01000111;
  Wire.write(val);
  Wire.endTransmission();
  
  /* Setup configuration register 12-bit */
     
  Wire.beginTransmission(THERM);  
  val = 1;  
  Wire.write(val);
  val = B01100000;
  Wire.write(val);
  Wire.endTransmission();
  
  /* Setup Digital THERMometer pointer register to 0 */
     
  Wire.beginTransmission(THERM); 
  val = 0;  
  Wire.write(val);
  Wire.endTransmission();
  
  /* Test 7-Segment */
  for (counter=0; counter<8; counter++)
  {
    Wire.beginTransmission(_7SEG);
    Wire.write(1);
    for (counter2=0; counter2<4; counter2++)
    {
      Wire.write(1<<counter);
    }
    Wire.endTransmission();
    delay (250);
  }
  
  while (1)
  {
      Wire.requestFrom(THERM, 2);
      Temperature_H = Wire.read();
      Temperature_L = Wire.read();
      
      /* Calculate temperature if not in standby mode and if not in time mode*/
      if (Report && !Time) {
        //get the temp
        Cal_temp (Decimal, Temperature_H, Temperature_L, IsPositive);
        //if arduino recording in ferinheit - convert back to celsius
        if(IsFer) Re_Cal(Decimal, Temperature_H, IsPositive, IsFer);
        
        /* Display temperature on the serial monitor. 
           Comment out this line if you don't use serial monitor.*/
        SerialMonitorPrint (Temperature_H, Decimal, IsPositive, IsFer);
        
        /* Update RGB LED.*/
        //updated so colors corespond more naturally to our intuition for ferinheit
        if (IsFer) UpdateRGB ((Temperature_H-32)*5.0/9.0);
        //standard UpdateRGB
        else UpdateRGB(Temperature_H);
        
        /* Display temperature on the 7-Segment */
        Dis_7SEG (Decimal, Temperature_H, Temperature_L, IsPositive, IsFer);
        
        delay (1000);        /* Take temperature read every 1 second */
      }
        
  
      //if the server has sent a message
      if (Serial.available() > 0){
        //get the input
        input = Serial.read();
       
        //input a 'F' for ferinheit
        if (input == 70) {
          //stop time mode
          Time = 0;
          //tick ferinehit flag
          IsFer = 1;
          
        //input a 'C' for celsius
        } else if (input == 67) {
          //stop time mode
          Time = 0;
          //untick ferinheit flag
          IsFer = 0;
            
        //input a 'S' to go into standby  
        } else if (input == 83) {
            //tick standby flag (Report = 1 indicates currently reporting, i.e. not in standby)
            Report = 0;
            //display 0s on the display
            Dis_7SEG (0, 0, 0, 1, 0);
            
        //input a 'R' to start reporting
        } else if (input == 82) {
            //exit standby by unticking flag
            Report = 1;
          
        //input a 'T' to start reporting the time    
        } else if (input == 84){
            //tick time mode flag
            Time = 1;
            //diplays 0's until time recieved
            Dis_7SEG (0, 0, 0, 1, 0);
            //a count of the bytes recieved from the server, 4 indicates the whole time
            count = 0;
            //this variable can probably be deleted.
            time_loop = 0;
          
        //input for a dot
        } else if (input == 46) {
            digitalWrite(RED, LOW);
            digitalWrite(GREEN, HIGH);
            digitalWrite(BLUE, HIGH);
            //short for a dot
            delay(250);
        //input for a dash
        } else if (input == 45) {
            digitalWrite(RED, HIGH);
            digitalWrite(GREEN, LOW);
            digitalWrite(BLUE, HIGH);
            //long for a dash
            delay(500);
            
        //when we are in time mode. wait for the time string
        } else if(Time == 1){
          //convert ascii int to ascii char
          time_char = input;
          //if we have the whole string and there is a "^" to indicate the end of the message recieved
          if (count == 4 && input == 94){
            //convert string to an int for the time
            time_value = atoi(time_string);
            //print the time on the display
            Dis_7SEG_Time(time_value);
            //prepare for next time message
            count = 0;
            //reset string for the next message
            strcpy(time_string,"");
          //if character is "$" reset varaibles and wait for time
          }else if (input == 36){
             strcpy(time_string,"");
             count = 0;            
           //if 4 characters recorded and the last is not "^" indicate that there was an error
          }else if (count > 4 || (count == 4 && input != 94)){
            //error count? but can be deleted
            time_value++;
            //diplay error on the display
            Dis_7SEG_Time(time_value);
            //reset and ready for next input
            strcpy(time_string,"");
            count = 0;
          //get the next time character
          }else{ 
            //get next time character
            time_string[count] = time_char;
            //null terminate the string
            time_string[count+1] = '\0';
            //increment string location
            count++;
          }
        }
      }    
   }
} 

/***************************************************************************
 Function Name: Cal_temp

 Purpose: 
   Calculate temperature from raw data.
****************************************************************************/
void Cal_temp (int& Decimal, byte& High, byte& Low, bool& sign)
{
  if ((High&B10000000)==0x80)    /* Check for negative temperature. */
    sign = 0;
  else
    sign = 1;
    
  High = High & B01111111;      /* Remove sign bit */
  Low = Low & B11110000;        /* Remove last 4 bits */
  Low = Low >> 4; 
  Decimal = Low;
  Decimal = Decimal * 625;      /* Each bit = 0.0625 degree C */
  
  if (sign == 0)                /* if temperature is negative */
  {
    High = High ^ B01111111;    /* Complement all of the bits, except the MSB */
    Decimal = Decimal ^ 0xFF;   /* Complement all of the bits */
  }  
}

//if the value was recorded in ferinheit, then convert to celsius as all values are saved in celsius
void Re_Cal(int& Decimal, byte& High, bool& sign, bool& IsFer)
{
    long Fer_Temp;
    int Fer_Temp_Decimal;
    long x = High;
    //make a single variable summing the whole numbers and the decimal.
    //convert that to ferinheit
    Fer_Temp = (x*10000 + Decimal)*9.0/5.0;
    //split off the decimal into a separate variable
    Fer_Temp_Decimal = Fer_Temp % 10000;
    //remove the decmial part from the summed variable
    Fer_Temp = Fer_Temp/10000;
    //push them back to the original variables
    Decimal = Fer_Temp_Decimal;
    High = Fer_Temp + sign*32 - (!sign)*32;
    //ensure that the sign is still correct
    if (High > 0) sign = 1;
    IsFer = 1;
}


/***************************************************************************
 Function Name: Dis_7SEG

 Purpose: 
   Display number on the 7-segment display.
****************************************************************************/

//copied from below just without the decimal part
//same function just posts the time instead of the temperature
void Dis_7SEG_Time(int time){
  byte Digit = 4;
  byte Number = 0;
  
  if (time > 999)
    Number = time/1000;
  Send7SEG(Digit,NumberLookup[Number]);
  Digit--;
  time = time % 1000;
  Number = 0;
  
  if (time > 99)
    Number = time/100;
  Send7SEG(Digit,NumberLookup[Number]);
  Digit--;
  time = time % 100;
  Number = 0;
  
  if (time>9)
    Number = time/10;
  Send7SEG(Digit,NumberLookup[Number]);
  Digit--;
  time = time % 10;
  Number = 0;
  
  Number = time;
  Send7SEG(Digit,NumberLookup[time]);
  
} 


void Dis_7SEG (int Decimal, byte High, byte Low, bool sign, bool IsFer)
{
  byte Digit = 4;                 /* Number of 7-Segment digit */
  byte Number;                    /* Temporary variable hold the number to display */
  
  if (sign == 0)                  /* When the temperature is negative */
  {
    Send7SEG(Digit,0x40);         /* Display "-" sign */
    Digit--;                      /* Decrement number of digit */
  }
  
  if (High > 99)                  /* When the temperature is three digits long */
  {
    Number = High / 100;          /* Get the hundredth digit */
    Send7SEG (Digit,NumberLookup[Number]);     /* Display on the 7-Segment */
    High = High % 100;            /* Remove the hundredth digit from the TempHi */
    Digit--;                      /* Subtract 1 digit */    
  }
  
  if (High > 9)
  {
    Number = High / 10;           /* Get the tenth digit */
    Send7SEG (Digit,NumberLookup[Number]);     /* Display on the 7-Segment */
    High = High % 10;            /* Remove the tenth digit from the TempHi */
    Digit--;                      /* Subtract 1 digit */
  }
  
  Number = High;                  /* Display the last digit */
  Number = NumberLookup [Number]; 
  if (Digit > 1)                  /* Display "." if it is not the last digit on 7-SEG */
  {
    Number = Number | B10000000;
  }
  Send7SEG (Digit,Number);  
  Digit--;                        /* Subtract 1 digit */
  
  if (Digit > 0)                  /* Display decimal point if there is more space on 7-SEG */
  {
    Number = Decimal / 1000;
    Send7SEG (Digit,NumberLookup[Number]);
    Digit--;
  }

  if (Digit > 0)                 /* Display "c" of "F" if there is more space on 7-SEG */
  {
    //if in celsius show a 'c'
    if(!IsFer && Report && !Time)
      Send7SEG (Digit,0x58);  //39
    //if in ferinheit show and "F'
    else if (IsFer && Report && !Time)
      Send7SEG (Digit,0x71);
    else
    //else report a "0"
      Send7SEG(Digit,NumberLookup[0]);
    Digit--;
  }
  
  //report "0"s at the end if there is more space
  if (Digit > 0)                 /* Clear the rest of the digit */
  {
    Send7SEG (Digit,NumberLookup[0]); 
    Digit--;  
  } 
  
  if (Digit > 0)                 /* Clear the rest of the digit */
  {
    Send7SEG (Digit,NumberLookup[0]); 
    Digit--;  
  } 
}

/***************************************************************************
 Function Name: Send7SEG

 Purpose: 
   Send I2C commands to drive 7-segment display.
****************************************************************************/

void Send7SEG (byte Digit, byte Number)
{
  Wire.beginTransmission(_7SEG);
  Wire.write(Digit);
  Wire.write(Number);
  Wire.endTransmission();
}

/***************************************************************************
 Function Name: UpdateRGB

 Purpose: 
   Update RGB LED according to define HOT and COLD temperature. 
****************************************************************************/

void UpdateRGB (byte Temperature_H)
{
  digitalWrite(RED, LOW);
  digitalWrite(GREEN, LOW);
  digitalWrite(BLUE, LOW);        /* Turn off all LEDs. */
  
  if (Temperature_H <= COLD)
  {
    digitalWrite(BLUE, HIGH);
  }
  else if (Temperature_H >= HOT)
  {
    digitalWrite(RED, HIGH);
  }
  else 
  {
    digitalWrite(GREEN, HIGH);
  }
}

/***************************************************************************
 Function Name: SerialMonitorPrint

 Purpose: 
   Print current read temperature to the serial monitor.
****************************************************************************/
void SerialMonitorPrint (byte Temperature_H, int Decimal, bool IsPositive, bool IsFer)
{
    //Serial.print("The temperature is ");
    if (!IsPositive)
    {
      Serial.print("-");
    }
    Serial.print(Temperature_H, DEC);
    Serial.print(".");
    Serial.print(Decimal, DEC);
    if (IsFer)
      Serial.println("F");
    else
      Serial.println("C");
}
    


