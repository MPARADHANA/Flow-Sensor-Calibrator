

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "Fonts/FreeSerifItalic9pt7b.h"
#include "TouchScreen.h"
#include "Timer.h"
#define Serial SerialUSB

//constants corresponding to the resistive touchscreen in 3.2" adafruit TFT display for the particular orientation used
#define TS_MINX 150
#define TS_MINY 120
#define TS_MAXX 920
#define TS_MAXY 940

#define MINPRESSURE 10
#define MAXPRESSURE 1000

// For the Adafruit shield, these are the default.
#define TFT_DC 41
#define TFT_CS 10
#define YP A1  // must be an analog pin
#define XM A0  // must be an analog pin
#define YM 23   // can be a digital pin
#define XP 25   // can be a digital pin

//  for the flow rate range, calibration duration relation{xmin,xmax,ymin,ymax,calibration duration(ms)} for each key
long rateSel[6][5] = {
  {5,105,50,130,60000},{110,210,50,130,30000},{215,315,50,130,15000},{5,105,135,215,10000},{110,210,135,215,7000},{215,315,135,215,1000}};





int px,py,pz,px1;//coordinates for the touchscreen
double flowrate;
double secondvalue = 0;
int everyReading, afterFlowtime; // IDs of timer events
long Flowtime = 1000;
double averageFlow;
int emergencyValue = 0;

const byte PumpRelayPin = 49;//digital pin 47 not working
const byte HeaterRelayPin = 45;
const byte ValveRelayPin = 51;
const byte interruptPin = 46;
const byte ledPin = 13;
volatile byte PumpState = LOW;// default off
volatile byte HeaterState = LOW;//default off
volatile byte ValveState = HIGH;// default open state, hence default powered
volatile byte LedState = LOW;


Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
Timer t;



double WFC,weightOld,weightNew,delWeight,del = 0.0;//for the flow computation


char incomingByte[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // for acquiring the weight data
char a = 0; //character read serially
int i = 0;//array index for obtaining serial data
char weight[16];

int fluidType;//1 for water glycerol solution and 0 for water
int no = 0;//count for carrying out the average at the end of a calibration cycle
int count = 0; //variable to keep track of the status of the display and process 
void setup() {
  Serial.begin(9600);
  Serial3.begin(9600);

  //configuring relay control pins as output
  pinMode(PumpRelayPin,OUTPUT);
  pinMode(HeaterRelayPin,OUTPUT);
  pinMode(ValveRelayPin, OUTPUT);

  //configuring switch(button) or interrupt pin as input
  pinMode(interruptPin, INPUT);

  //configuring led pin as output
  pinMode(ledPin, OUTPUT);
  
  attachInterrupt(digitalPinToInterrupt(interruptPin), emergency, RISING);


  tft.begin();
  
  tft.setRotation(3); //to set the particular landscape orientation of the display
  tft.setFont(&FreeSerifItalic9pt7b); //to set the font style

  //starting screen layout
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(5,70,310, 50, ILI9341_BLUE); 
  tft.fillRect(5,15,310, 50, ILI9341_WHITE);
  tft.setCursor(30,45);

  tft.setTextColor(ILI9341_BLACK);  tft.setTextSize(1);
  tft.print("FLOW SENSOR CALIBRATION");
  tft.setCursor(45,100);
  tft.print("Power on the weighing scale"); 
  while(!Serial3.available()){
      ;// waiting for the establishment of serial communication between the weighing scale and the arduino due
  }
  tft.fillRect(110,140,120,60, ILI9341_GREEN);
  tft.setCursor(150,170);
  tft.print("Next");
}
void loop(){
    TSPoint p = ts.getPoint(); // acquiring the coordinates of the point of contact on the touchscreen
    t.update(); // for the timer events to happen continuously

    //mapping of the touchscreen coordinates to the pixels of the display along the display orientation of the screen 
    p.x = map(p.x, TS_MINX, TS_MAXX, 0, 240);
    p.y = map(p.y, TS_MINY, TS_MAXY, 0, 320);
    px = p.y;
    py = p.x;
    pz = p.z;
    px1 = 320 - px;

    //for writing the status of the relay control pins and led pin
    digitalWrite(PumpRelayPin, PumpState);
    digitalWrite(HeaterRelayPin,HeaterState);
    digitalWrite(ValveRelayPin, ValveState);
    digitalWrite(ledPin, LedState);
    
    //getting weight data
    if (Serial3.available() > 0) {
               
              
                a = Serial3.read();
                if((a!='\n')&&(a!='\r')&&(a!='g')&&(a!='-')){
                incomingByte[i]=a; // reading character by character and storing it in an array
                i++;
                }
                else{
                i=0;
                
                //Serial.print("Weight: ");
                //Serial.println(incomingByte);
                WFC = atof(incomingByte); // conversion of the character array to a double value
                //Serial.print("Weightx: ");
                //Serial.println(WFC);
                }
              
               
        }
    if(count == -1)
    {
      screenStart();
    }
    if((WFC==0)&&(count==1))
    {
      screen1new(); // for checking if the weighing scale has been tared
      count = 2;
    }



    // for button presses on the touchscreen display 
    if((pz > MINPRESSURE)&&(pz<MAXPRESSURE))
    {
      if(count ==0)
      {
        screen0Check(px1,py); // to check if 'next' button has been pressed in the first screen or screen 0
        
      } 
      else if(count == 2)
      {
        screen1Check(px1,py);// to check if 'next' has been pressed after taring the scale
      }
      else if(count == 3)
      {
        screen2Check(px1,py);//for fluid button
      }
      else if(count == 4)
      {
        screen3Check(px1,py);//for flow rate range selection
        
      }
      /*else if(count == 5)
      {
        screen4Check(px1,py);//confirmation of flow rate         This was used when keypad was used as the method of input for flowrate
       
      }*/
      else if(count == 6)
      {
        screen5Check(px1,py);//selection of temperature
       
      }
      else if(count == 7)
      {
        screen6Check(px1,py);//check for start calibration and back 
       
      }
      else if(count == 8)
      {
        screen8Check(px1,py);//final result display
       
      }
      else if(count == 9)
      {
        screen9Check(px1,py);//Emptying the tank
       
      }
      
      
    } 
 }


 void screen0Check(int px1, int py)
 {
   if((px1>110)&&(px1<230)&&(py>120)&&(py<180))
   {
    screen1();
    count = 1;
   }
   return;
 }

 
 void screen1()
 {
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(5,70,310, 50, ILI9341_BLUE); 
  tft.fillRect(5,15,310, 50, ILI9341_WHITE);
  tft.setCursor(30,45);

  tft.setTextColor(ILI9341_BLACK);  tft.setTextSize(1);
  tft.print("FLOW SENSOR CALIBRATION");
  tft.setCursor(55,100);
  tft.print("Press the TARE button"); 
  return;
 }


 void screen1new()
 {
  tft.fillScreen(ILI9341_BLACK);
  
  tft.fillRect(5,15,310, 50, ILI9341_WHITE);
  tft.setCursor(30,45);

  tft.setTextColor(ILI9341_BLACK);  tft.setTextSize(1);
  tft.print("FLOW SENSOR CALIBRATION");
  
  tft.fillRect(110,140,120,60, ILI9341_GREEN);
  tft.setCursor(150,170);
  tft.print("Next");
  return;
 }


 void screen1Check(int px1, int py)
 {
   if((px1>110)&&(px1<230)&&(py>120)&&(py<180))
   {
    screen2();
    count = 3;
   }
   return;
 }


 void screen2()
 {
  PumpState = HIGH;
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(5,70,310, 50, ILI9341_BLUE); 
  tft.fillRect(5,15,310, 50, ILI9341_WHITE);
  tft.setCursor(140,45);

  tft.setTextColor(ILI9341_BLACK);  tft.setTextSize(1);
  tft.print("FLUID");
  tft.setCursor(110,100);
  tft.print("Select the fluid"); 
  tft.fillRect(40,140,120,60, ILI9341_GREEN);
  tft.fillRect(180,140,120,60, ILI9341_GREEN);
  tft.setCursor(70,170);
  tft.print("Water");
  tft.setCursor(210,155);
  tft.print("Water");
  tft.setCursor(202,175);
  tft.print("Glycerol");
  tft.setCursor(202,195);
  tft.print("Solution");
  return;
 }

 void screen2Check(int px1,int py)
 {
    if((px1>40)&&(px1<160)&&(py>140)&&(py<200))
    {
      screen3();
      count = 4;
      fluidType = 0;
    }
    else if((px1>180)&&(px1<300)&&(py>140)&&(py<200))
    {
      screen3();
      count = 4;
      fluidType = 1;
    }
    return;
 }

 void screen3()
 {
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(5,5,310, 45, ILI9341_WHITE);
  tft.setCursor(7,40);

  tft.setTextColor(ILI9341_BLACK);  tft.setTextSize(2);
  tft.print("Set Flow Rate(mL/min)");
  tft.fillRect(5,50,100,80, ILI9341_GREEN);
  tft.fillRect(110,50,100,80, ILI9341_GREEN);
  tft.fillRect(215,50,100,80, ILI9341_GREEN);
  tft.fillRect(5,135,100,80, ILI9341_GREEN);
  tft.fillRect(110,135,100,80, ILI9341_GREEN);
  tft.fillRect(215,135,100,80, ILI9341_GREEN);
 
  tft.setCursor(40,100);

  tft.setTextColor(ILI9341_BLACK);  tft.setTextSize(1);
  tft.print("<5000");  
  tft.setCursor(115,100);
  tft.print("5001-14999");
  tft.setCursor(217,100);
  tft.print("15000-19999"); 
  tft.setCursor(10,185);
  tft.print("20000-29999");  
  tft.setCursor(115,185);
  tft.print("30000-40000");
  tft.setCursor(225,185);
  tft.print(">40000"); 
  return;
 }


void screen3Check(int px1, int py)
{
      for(int i = 0;i<6;i++){
          if((px1>rateSel[i][0])&&(px1<rateSel[i][1])&&(py>rateSel[i][2])&&(py<rateSel[i][3])){
            Flowtime = rateSel[i][4];
            count = 6;
            screen5();
            break;}
      } 
      
      return;
}

void screen4()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(5,70,310, 50, ILI9341_BLUE); 
  tft.fillRect(5,15,310, 50, ILI9341_WHITE);
  tft.setCursor(25,45);

  tft.setTextColor(ILI9341_BLACK);  tft.setTextSize(2);
  tft.print("Flow Rate(mL/min)");
  tft.setCursor(100,100);
  tft.setTextColor(ILI9341_BLACK);  tft.setTextSize(2);
  tft.print(flowrate);
  tft.fillRect(40,140,120,60, ILI9341_RED);
  tft.fillRect(180,140,120,60, ILI9341_GREEN);
  tft.setTextSize(1);
  tft.setCursor(70,170);
  tft.print("Back");
  tft.setCursor(210,170);
  tft.print("Next");
  return;
}

void screen4Check(int px1, int py)
{
  if((px1>40)&&(px1<160)&&(py>140)&&(py<200))
    { 
      screen3();
      count = 4;  
    }
    else if((px1>180)&&(px1<300)&&(py>140)&&(py<200))
    {
       screen5();
      count = 6;
      
    }
    return;
}

void screen5()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(5,70,310, 50, ILI9341_BLUE); 
  tft.fillRect(5,15,310, 50, ILI9341_WHITE);
  tft.setCursor(100,45);

  tft.setTextColor(ILI9341_BLACK);  tft.setTextSize(1);
  tft.print("TEMPERATURE");
  tft.setCursor(90,100);
  tft.print("Select the temperature"); 
  tft.fillRect(5,140,150,80, ILI9341_GREEN);
  tft.fillRect(160,140,150,80, ILI9341_GREEN);
  tft.setCursor(20,190);
  tft.print("Set Temperature");
  tft.setCursor(170,190);
  tft.print("Room Temperature");
  return;
  
}

void screen5Check(int px1, int py)
{
  if((px1>5)&&(px1<155)&&(py>140)&&(py<220))
    { 
      screen6a();
      count = 7;
      HeaterState=HIGH;  
    }
    else if((px1>160)&&(px1<310)&&(py>140)&&(py<220))
    {
      screen6b();
      count = 7;
      
    }
    return;
}

void screen6a()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(5,70,310, 50, ILI9341_BLUE); 
  tft.fillRect(5,15,310, 50, ILI9341_WHITE);
  tft.setCursor(10,45);

  tft.setTextColor(ILI9341_BLACK);  tft.setTextSize(1);
  tft.print("CALIBRATION AT SET TEMPERATURE");
  tft.setCursor(15,85);
  tft.print("Wait until the required temperature is");
  tft.setCursor(150,105);
  tft.print("attained");
  tft.fillRect(5,180,80,60, ILI9341_RED);
  tft.fillRect(100,140,135,80, ILI9341_GREEN);
  tft.setCursor(30,215);
  tft.print("Back");
  tft.setCursor(110,180);
  tft.print("Start Calibration");
  return;
}


void screen6b()
{
  tft.fillScreen(ILI9341_BLACK);
  //tft.fillRect(5,70,310, 50, ILI9341_BLUE); 
  tft.fillRect(5,15,310, 50, ILI9341_WHITE);
  tft.setCursor(10,35);

  tft.setTextColor(ILI9341_BLACK);  tft.setTextSize(1);
  tft.print("CALIBRATION AT ROOM");
  tft.setCursor(160,55);
  tft.print("TEMPERATURE");
  tft.fillRect(5,180,80,60, ILI9341_RED);
  tft.fillRect(100,140,135,80, ILI9341_GREEN);
  tft.setCursor(30,215);
  tft.print("Back");
  tft.setCursor(110,180);
  tft.print("Start Calibration");
  return;
}

void screen6Check(int px1, int py)
{
  if((px1>5)&&(px1<85)&&(py>180)&&(py<240))
    { 
      screen5();
      count = 6;  
    }
    else if((px1>100)&&(px1<235)&&(py>140)&&(py<220))
    { 
      
      screen7();
      count = 8;
      ValveState = LOW;
      digitalWrite(ValveRelayPin, ValveState);
      delay(2000);
      
  
      count = 8;
     
      timing();
    }
    return;
}


void timing()
{ 
  
  everyReading = t.every(1000,takeReading); //executes the function 'takeReading' every one second and hence calculates flow rate every one second
 
  afterFlowtime = t.after(Flowtime,average); // after Flowtime, executes the function 'average' 
   
}

void takeReading()
{ 
  weightOld = weightNew;
  for(int j=0;j<16;j++)
  { 
    if (Serial3.available() > 0) {
               
              
                a = Serial3.read();
                if((a!='\n')&&(a!='\r')&&(a!='g')){
                incomingByte[i]=a;
                i++;
                }
                else{
                i=0;
                
                //Serial.print("Weight: ");
                //Serial.println(incomingByte);
                WFC = atof(incomingByte);
                //Serial.print("Weightx: ");
                //Serial.println(WFC);
                }
              
               
        }
  }
  weightNew = WFC;
  del = weightNew - weightOld; //in one second
  
  if(del>0)
  {
    secondvalue = del*60; // for convertion from g/s to g/min
    if(no!=0) // to avoid first value, as it generally had large difference from others values due to high initial impact and unstability
    {
      delWeight = delWeight + del; // for average computation
    }
    no = no +1;
    Serial.println(secondvalue);
  }
  //Serial.print("weightOld = ");
  //Serial.println(weightOld);
  //Serial.print("weightNew = ");
  //Serial.println(weightNew);
  //Serial.print("flowrate = ");
  
  //no++;
  return;
}

void average()
{
  t.stop(everyReading); // stops the timer event everyReading
  averageFlow = delWeight*60/(no-1);
  Serial.print("average flow = ");
  Serial.print(averageFlow);
  Serial.println(" g/min");
  ValveState = HIGH;
  HeaterState = LOW;
  PumpState = LOW;
  LedState = LOW;
  digitalWrite(ValveRelayPin, ValveState);
  digitalWrite(HeaterRelayPin, HeaterState);
  digitalWrite(PumpRelayPin, PumpState);
  digitalWrite(ledPin, LedState);
  screen8();
  return;
}

void screen7()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(5,5,310,310, ILI9341_WHITE);
  tft.setCursor(110,100);
  tft.setTextSize(2);
  tft.print("Calibrating.....");
  LedState = HIGH;
  return;
  
  
}

void screen8()
{ 
  count = 8;
  tft.fillScreen(ILI9341_BLACK);tft.setTextSize(1);
  tft.fillRect(5,15,310, 50, ILI9341_WHITE);
  tft.fillRect(5,70,310, 100, ILI9341_BLUE);
  tft.setCursor(130,45);
  tft.print("RESULT");
  tft.setCursor(130,125);
  tft.print(averageFlow);
  tft.print(" g/min");
  tft.fillRect(50,185,100,50,ILI9341_GREEN);
  tft.fillRect(170,185,100,50,ILI9341_GREEN);
  
  tft.setCursor(80,215);tft.setTextSize(1);
  tft.print("Reset");
  tft.setCursor(210,215);
  tft.print("End");
  return;
 
}

void screen8Check(int px1, int py)
{
  if((px1>50)&&(px1<150)&&(py>185)&&(py<235))
    { 
      screen3();
      PumpState = HIGH;
      digitalWrite(PumpRelayPin, PumpState);
      count = 4; 
      no = 0;
      delWeight = 0;
      
    }
   else if((px1>170)&&(px1<270)&&(py>185)&&(py<235))
   {
    screen9();
    count = 9;
   }
    return;
}

void screenStart()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(5,70,310, 50, ILI9341_BLUE); 
  tft.fillRect(5,15,310, 50, ILI9341_WHITE);
  tft.setCursor(30,45);

  tft.setTextColor(ILI9341_BLACK);  tft.setTextSize(1);
  tft.print("FLOW SENSOR CALIBRATION");
  tft.setCursor(45,100);
  tft.print("Power on the weighing scale"); 
  while(!Serial3.available()){
      ;
  }
  count =0;
  tft.fillRect(110,140,120,60, ILI9341_GREEN);
  tft.setCursor(150,170);
  tft.print("Next");
}

void screen9()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(5,70,310,80, ILI9341_BLUE); 
  tft.fillRect(5,15,310, 50, ILI9341_WHITE);
   tft.setCursor(30,45);

  tft.setTextColor(ILI9341_BLACK);  tft.setTextSize(1);
  tft.print("SHUT DOWN");
  tft.setCursor(10,90);
  tft.println(" Turn the 3 way valve to remove water");
  tft.setCursor(10,110);
  tft.println( "After turning the valve, click on the 'Empty' button");
  tft.fillRect(120,170,80,60, ILI9341_GREEN);
  tft.setCursor(140,200);
  tft.print("Empty");
  return; 
}

void screen9Check(int px1, int py)
 {
   if((px1>120)&&(px1<200)&&(py>170)&&(py<230))
   {
    screen10();
    PumpState = HIGH;
    //count = -1;
   }
   return;
 }

 void screen10()
 {
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(5,70,310, 120, ILI9341_BLUE); 
  tft.fillRect(5,15,310, 50, ILI9341_WHITE);
   tft.setCursor(30,45);

  tft.setTextColor(ILI9341_BLACK);  tft.setTextSize(1);
  tft.print("SHUT DOWN");
  tft.setCursor(10,90);
 
  if( fluidType ==1)//if water glycerol solution
  {
    tft.println( "Clean the test bench!");
  }
  tft.setCursor(10,110);
  tft.println("Please power off");
  return;
 }

 void emergency() // interrupt function to be executed on pressing the emergency button
 {
  
  PumpState = LOW; // turn off pump
  HeaterState = LOW;//turn off heater
  ValveState = HIGH;//open the valve
  LedState = LOW;

   digitalWrite(PumpRelayPin, PumpState);
   digitalWrite(HeaterRelayPin,HeaterState);
   digitalWrite(ValveRelayPin, ValveState);
   digitalWrite(ledPin, LedState);
   if(everyReading)
   {
     
     t.stop(everyReading);
     t.stop(afterFlowtime);
   }
   count = 8;
   average(); // to display the result based on the values obtained till then
   emergencyValue = 1;
   //screen8();
   
   return;
 }
