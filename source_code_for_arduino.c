/*
 *  HVAC car control with Arduino
 *  Version 1.0
 *  Copyright (C) 2018  Hartmut Wendt  www.hwhardsoft.de
 *  
 *   
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/  


#include <OneWire.h>
#include <TimerOne.h> 


// Portkonfiguration
#define REL_VENT_FRONT 5      //relay output for ventilation front
#define REL_VENT_FOOT 6       //relay output for ventilation footwell area
#define REL_VENT_WINDOW 7     //relay output for ventilation window
#define REL_FAN_PWR 8             //relay output for fan power
#define PWM_FAN_PIN 9             //PWM output for fan power
#define REL_COMPRESSOR 11         //relay output for AC compressorr
#define REL_HEATING 12            //relay output for heating valve
#define REL_AIR_CIRCULATION 13    //relay output for air circulation
#define REL_FOG_LIGHT 14          //relay output for fog light
#define REL_HEATING_WINDOW_FRONT 15   //relay output for window heating front
#define REL_HEATING_WINDOW_REAR 16    //relay output for window heating rear
#define TEST_PIN 19               // output for sw tests

// OneWire DS18S20, DS18B20, DS1822 Temperature sensor
OneWire  sens_out(2);       // outside temperature sensor on D2
OneWire  sens_in_right(3);  // inside right side temperature sensor on D3
OneWire  sens_in_left(4);   // inside left side temperature sensor on D4

int temp_out = 20;             // measured outside temperature
int temp_in_right = 20;        // measured inside right side temperature
int temp_in_left = 20;         // measured inside left side temperature
int set_in_right = 20;        // setted inside right side temperature
int set_in_left = 20;         // setted inside left side temperature



// Variables
int i1;
char *test;
String s1;
String inputString;
boolean AC_ENABLED = false;
int bscheduler = 0;

/** Wird beim Start einmal ausgefhrt */
void setup()
{
    // set port direction
    pinMode(REL_VENT_FRONT, OUTPUT);
    pinMode(REL_VENT_FOOT, OUTPUT);
    pinMode(REL_VENT_WINDOW, OUTPUT);    
    pinMode(REL_FAN_PWR, OUTPUT);
    pinMode(REL_COMPRESSOR, OUTPUT);
    pinMode(REL_HEATING, OUTPUT);
    pinMode(REL_AIR_CIRCULATION, OUTPUT);
    pinMode(REL_FOG_LIGHT, OUTPUT);
    pinMode(REL_HEATING_WINDOW_FRONT, OUTPUT);
    pinMode(REL_HEATING_WINDOW_REAR, OUTPUT);  
    pinMode(TEST_PIN, OUTPUT);  

    // init serial port for nextion communication.
    Serial.begin(9600);   
    delay(250);
    
    // first read in of temperatures
    temp_out = read_temperature(sens_out);
    delay(100);
    temp_in_right = read_temperature(sens_in_right);
    delay(100);
    temp_in_left = read_temperature(sens_in_left);
    
    //pwm init
    Timer1.initialize(1000);  // 1.000 us = 1 kHz 
    Timer1.pwm (PWM_FAN_PIN, 0);
   
}


// ----- Main loop -----------------------------------------------------------------------------------
void loop()
{
    // processing of incomming messages from nextion
    Nextion_processing();

    switch(bscheduler) 
    {

      // measure outside temperature
      case 0: 
          temp_out = read_temperature(sens_out);
          // transmit outside temperature to nextion display 
          Serial.print("ID1.val=");    
          Serial.print(temp_out);            
          Serial.write(0xff);
          Serial.write(0xff);
          Serial.write(0xff);        
          break;

      // measure inside right temperature
      case 25: 
          temp_in_right = read_temperature(sens_in_right);
          break;
          
      // measure inside left temperature
      case 50: 
          temp_in_left = read_temperature(sens_in_left);
          break;

      // HVAC control
      case 75: 
        clima_control();
        break;

      
    }
    bscheduler++;
    if (bscheduler > 100) bscheduler = 0;   

    delay(10);


}


// read temperature from a connected 1wire temperature sensor
int read_temperature(OneWire ds) {
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];

  if ( !ds.search(addr)) 
  {
    ds.reset_search();
    delay(250);
    return(97);
  }
 
  if (OneWire::crc8(addr, 7) != addr[7]) 
  {      
      return(98);
  }
 
 
  // the first ROM byte indicates which chip
  switch (addr[0]) 
  {
    case 0x10:
      type_s = 1;
      break;
    case 0x28:
      type_s = 0;
      break;
    case 0x22:
      type_s = 0;
      break;
    default:      
      return(99);
  } 
 
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end  
  delay(750);
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad
 
  for ( i = 0; i < 9; i++) 
  {           
    data[i] = ds.read();
  }
 
  // Convert the data to actual temperature
  int16_t raw = (data[1] << 8) | data[0];
 
  if (type_s) {   
    
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) 
    {
      digitalWrite(TEST_PIN, HIGH);
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } 
  else 
  {
    byte cfg = (data[4] & 0x60);
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
 
  } 

  return((int)raw / 16.0); 
}





unsigned long StrToHex(String str, byte digits)
{
  char ConvByte[10];
  str.toCharArray(ConvByte, digits);
  return (unsigned long) strtol(ConvByte, NULL, 16);
}


// ------ receiving and processing of incomming data from nextion
void Nextion_processing()
{
  int i1;
  String s1;
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    if (inChar != '|') return;
    }  

  int i2=inputString.indexOf("=");
  if (i2 > 0) {    
    #ifdef _debug_snd
    Serial.print("Nextion: ");
    Serial.print(inputString);
    Serial.println("    -   ");
    Serial.print(inputString.substring(i2-2,i2));
    Serial.print("=");
    Serial.println(inputString[i2+1]);                 
    #endif      

    //- id2 FOG LIGHT -
    //------------------------------------------------------------------------------------------------
    if (inputString.indexOf("id02=1")> -1) {        
      // FOG LIGHT on
      digitalWrite(REL_FOG_LIGHT, HIGH);
      
    } 
    else if(inputString.indexOf("id02=0")> -1) {
      // FOG LIGHT off
      digitalWrite(REL_FOG_LIGHT, LOW);
    }        

    //- id5 ventilation footwell + window -
    //------------------------------------------------------------------------------------------------
    if (inputString.indexOf("id05=1")> -1) {        
      // set ventilation to foot well & window
      digitalWrite(REL_VENT_FRONT, LOW);
      digitalWrite(REL_VENT_FOOT, HIGH);
      digitalWrite(REL_VENT_WINDOW, HIGH);
      
    } 
    else if(inputString.indexOf("id05=0")> -1) {
      // switch all ventilation off
      digitalWrite(REL_VENT_FRONT, LOW);
      digitalWrite(REL_VENT_FOOT, LOW);
      digitalWrite(REL_VENT_WINDOW, LOW);
    }        

    //- id9 ventilation footwell -
    //------------------------------------------------------------------------------------------------
    if (inputString.indexOf("id09=1")> -1) {        
      // set ventilation to foot well
      digitalWrite(REL_VENT_FRONT, LOW);
      digitalWrite(REL_VENT_FOOT, HIGH);
      digitalWrite(REL_VENT_WINDOW, LOW);
      
    } 
    else if(inputString.indexOf("id09=0")> -1) {
      // switch all ventilation off
      digitalWrite(REL_VENT_FRONT, LOW);
      digitalWrite(REL_VENT_FOOT, LOW);
      digitalWrite(REL_VENT_WINDOW, LOW);
    }        

    //- id14 ventilation front -
    //------------------------------------------------------------------------------------------------
    if (inputString.indexOf("id14=1")> -1) {        
      // set ventilation to front
      digitalWrite(REL_VENT_FRONT, HIGH);
      digitalWrite(REL_VENT_FOOT, LOW);
      digitalWrite(REL_VENT_WINDOW, LOW);
      
    } 
    else if(inputString.indexOf("id14=0")> -1) {
      // switch all ventilation off
      digitalWrite(REL_VENT_FRONT, LOW);
      digitalWrite(REL_VENT_FOOT, LOW);
      digitalWrite(REL_VENT_WINDOW, LOW);
    }        

    //- id22 ventilation front + footwell -
    //------------------------------------------------------------------------------------------------
    if (inputString.indexOf("id22=1")> -1) {        
      // set ventilation to front + foot well
      digitalWrite(REL_VENT_FRONT, HIGH);
      digitalWrite(REL_VENT_FOOT, HIGH);
      digitalWrite(REL_VENT_WINDOW, LOW);
      
    } 
    else if(inputString.indexOf("id22=0")> -1) {
      // switch all ventilation off
      digitalWrite(REL_VENT_FRONT, LOW);
      digitalWrite(REL_VENT_FOOT, LOW);
      digitalWrite(REL_VENT_WINDOW, LOW);
    }            

    //- id4 electrical window heating rear -
    //------------------------------------------------------------------------------------------------
    if (inputString.indexOf("id04=1")> -1) {        
      // window heating on
      digitalWrite(REL_HEATING_WINDOW_REAR, HIGH);
      
    } 
    else if(inputString.indexOf("id04=0")> -1) {
      // window heating off
      digitalWrite(REL_HEATING_WINDOW_REAR, LOW);
    }  

    //- id6 electrical window heating front -
    //------------------------------------------------------------------------------------------------
    if (inputString.indexOf("id06=1")> -1) {        
      // window heating on
      digitalWrite(REL_HEATING_WINDOW_FRONT, HIGH);
      
    } 
    else if(inputString.indexOf("id06=0")> -1) {
      // window heating off
      digitalWrite(REL_HEATING_WINDOW_FRONT, LOW);
    }  


    //- id7 air circulation -
    //------------------------------------------------------------------------------------------------
    if (inputString.indexOf("id07=1")> -1) {        
      // air circulation on
      digitalWrite(REL_AIR_CIRCULATION, HIGH);
      
    } 
    else if(inputString.indexOf("id07=0")> -1) {
      //air circulation off
      digitalWrite(REL_AIR_CIRCULATION, LOW);
    }  


    //- id10 Air Condition enabled/disabled -
    //------------------------------------------------------------------------------------------------
    if (inputString.indexOf("id10=1")> -1) {        
      // AC enabled
      AC_ENABLED = true;
    } 
    else if(inputString.indexOf("id10=0")> -1) {
      // AC disabled
      AC_ENABLED = false;
    }  


    //- id18 HVAC on/off -
    //------------------------------------------------------------------------------------------------
    if (inputString.indexOf("id18=1")> -1) {        
      // HVAC on
      
    } 
    else if(inputString.indexOf("id18=0")> -1) {
      // HVAC off
      AC_ENABLED = false;
      
      // FAN off
      digitalWrite(REL_FAN_PWR, LOW);
      Timer1.pwm (PWM_FAN_PIN, 0);
      
      //air circulation off
      digitalWrite(REL_AIR_CIRCULATION, LOW);

      // window heating off
      digitalWrite(REL_HEATING_WINDOW_REAR, LOW);
      digitalWrite(REL_HEATING_WINDOW_FRONT, LOW);

      // switch all ventilation off
      digitalWrite(REL_VENT_FRONT, LOW);
      digitalWrite(REL_VENT_FOOT, LOW);
      digitalWrite(REL_VENT_WINDOW, LOW);     
    }  


    //- id13 FAN control -
    //------------------------------------------------------------------------------------------------
    if (inputString.indexOf("id13=0")> -1) {        
      // FAN off
      digitalWrite(REL_FAN_PWR, LOW);
      Timer1.pwm (PWM_FAN_PIN, 0);
    } 
    else if(inputString.indexOf("id13=1")> -1) {
      // FAN level 1
      digitalWrite(REL_FAN_PWR, HIGH);
      Timer1.pwm (PWM_FAN_PIN, 146);
    }  
    else if(inputString.indexOf("id13=2")> -1) {
      // FAN level 2
      digitalWrite(REL_FAN_PWR, HIGH);
      Timer1.pwm (PWM_FAN_PIN, 293);
    }  
    else if(inputString.indexOf("id13=3")> -1) {
      // FAN level 3
      digitalWrite(REL_FAN_PWR, HIGH);
      Timer1.pwm (PWM_FAN_PIN, 439);
    }  
    else if(inputString.indexOf("id13=4")> -1) {
      // FAN level 4
      digitalWrite(REL_FAN_PWR, HIGH);
      Timer1.pwm (PWM_FAN_PIN, 585);
    }  
    else if(inputString.indexOf("id13=5")> -1) {
      // FAN level 5
      digitalWrite(REL_FAN_PWR, HIGH);
      Timer1.pwm (PWM_FAN_PIN, 731);
    }  
    else if(inputString.indexOf("id13=6")> -1) {
      // FAN level 6
      digitalWrite(REL_FAN_PWR, HIGH);
      Timer1.pwm (PWM_FAN_PIN, 877);
    }  
    else if(inputString.indexOf("id13=7")> -1) {
      // FAN level Max
      digitalWrite(REL_FAN_PWR, HIGH);
      Timer1.pwm (PWM_FAN_PIN, 1023);
    }  

    //-- setted temperature right --    
    //------------------------------------------------------------------------------------------------    
    i1 = inputString.indexOf("id20=");
    if (i1 > -1)
    { 
      set_in_right = inputString.charAt(i1 + 5) - 48;       
    }

    //-- setted temperature left --    
    //------------------------------------------------------------------------------------------------    
    i1 = inputString.indexOf("id16=");
    if (i1 > -1)
    { 
      set_in_left = inputString.charAt(i1 + 5) - 48;  
    }

                  
  }
    
  while(Serial.available()) {Serial.read();}
  inputString = "";

}


// HVAC control
void clima_control() {
    
    // cooling or heating requiredn?    
    if (temp_out < set_in_left) {
      // heating
      digitalWrite(REL_COMPRESSOR, LOW);
      digitalWrite(REL_HEATING, HIGH);

   
    } else if ((temp_out > (set_in_left + 1)) && (temp_out > 15) && (AC_ENABLED == true)) {
      // cooling 
      digitalWrite(REL_COMPRESSOR, HIGH);
      digitalWrite(REL_HEATING, LOW);

    
    } else {
      // temperature ok - no further action required
      digitalWrite(REL_COMPRESSOR, LOW);
      digitalWrite(REL_HEATING, LOW);
    }
}


