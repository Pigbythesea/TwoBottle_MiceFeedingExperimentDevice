/*
  TwoBottle Feeding Device library  –  liquid-reward fork of FED3
  ----------------------------------------------------------------
  Author      : Zihuan Zhang  <zzhan330@jh.edu>
  Repository  : https://github.com/Pigbythesea/TwoBottle_FeedingDevice.git
  First release: August 2025

  This library drives a dual-bottle liquid reward rig based on a Teensy 4.1.
  It inherits most hardware support from the original FED3 project
  (pellet feeder by Lex Kravitz et al., 2019-2021) and adds:

    • High-speed stepper control for 10 µL dispenses  
    • Independent left/right lick sensing via MPR121  
    • Progressive- and fixed-ratio liquid schedules    

  Acknowledgements
  ----------------
  • Lex Kravitz & Eric Lin – creators of the FED3 hardware and library  
  • Adafruit Industries – breakout boards & driver code used throughout  

  License
  -------
  Released under the Apache License, Version 2.0
  (http://www.apache.org/licenses/LICENSE-2.0) with the following  
  Copyright © 2025 Zihuan Zhang
*/


/**************************************************************************************************************************************************
                                                                                                    Startup stuff
**************************************************************************************************************************************************/
#include "Arduino.h"
#include <TimeLib.h>
#include <imxrt.h>
#include "TwoBottle.h"
TwoWire* mprWire = &Wire2;
#include <core_pins.h>
static inline void teensyReset() {
  // Reset the Teensy microcontroller
  SCB_AIRCR = 0x05FA0004; // Write the reset value to the Application Interrupt and Reset Control Register (AIRCR)
  while (1);
}


//  Start FED3 and RTC objects
FED3 *pointerToFED3;


//  Interrupt handlers

static void outsideLeftTriggerHandler(void) {
  pointerToFED3->leftTrigger();
}

static void outsideRightTriggerHandler(void) {
  pointerToFED3->rightTrigger();
}

static void outsideLickIRQ(void) {
  pointerToFED3->lickIRQ = true; // Set the lick interrupt flag
}

/**************************************************************************************************************************************************
                                                                                                        Main loop
**************************************************************************************************************************************************/
void FED3::run() {
  //This should be called at least once per loop.  It updates the time, updates display, and controls sleep 
  if (lickIRQ) serviceLicks();
  if (leftHeld  && digitalRead(LEFT_POKE)  == HIGH) leftHeld  = false;
  if (rightHeld && digitalRead(RIGHT_POKE) == HIGH) rightHeld = false;
  time_t nowTime = now();
  currentHour = hour(nowTime); //useful for timed feeding sessions
  currentMinute = minute(nowTime); //useful for timed feeding sessions
  currentSecond = second(nowTime); //useful for timed feeding sessions
  unixtime = nowTime;
  ReadBatteryLevel();
  UpdateDisplay();
  goToSleep();
}

/**************************************************************************************************************************************************
                                                                                                Poke functions
**************************************************************************************************************************************************/
//log left poke
void FED3::logLeftPoke(){
    Left = false;
    Serial.println(">>> [FED3] logLeftPoke() start");
    leftPokeTime = millis();
    LeftCount ++;
    leftInterval = 0.0;
    if (digitalRead(LEFT_POKE) == LOW) return;  //Hang here until poke is clear
    leftInterval = (millis()-leftPokeTime);
    UpdateDisplay();
    DisplayLeftInt();
    if (leftInterval < minPokeTime) {
      Event = "LeftShort";
    }
    else{
      Event = "LeftPoke";
    }

    logdata();
    LeftDropAvailable = false; //reset left drop
}

//log right poke
void FED3::logRightPoke(){
    Right = false; 
    rightPokeTime = millis();
    RightCount ++;
    rightInterval = 0.0;
    if (digitalRead (RIGHT_POKE) == LOW) return;  //Hang here until poke is clear
    rightInterval = (millis()-rightPokeTime);
    UpdateDisplay();
    DisplayRightInt();
    if (rightInterval < minPokeTime) {
      Event = "RightShort";
    }
    else{
      Event = "RightPoke";
    }

    logdata();
    RightDropAvailable = false; //reset right drop
}

void FED3::logLeftLick(){
  Event = "LeftLick";
  UpdateDisplay();
  logdata();
}

void FED3::logRightLick(){
  Event = "RightLick";
  UpdateDisplay();
  logdata();
}

void FED3::randomizeActivePoke(int max){
  //Store last active side and randomize
  byte lastActive = activePoke;
  activePoke = random (0, 2);

  //Increment consecutive active pokes, or reset consecutive to zero
  if (activePoke == lastActive) {
    consecutive ++;
  }
  else {
    consecutive = 0;
  }
  
  //if consecutive pokes are too many, swap pokes
  if (consecutive >= max){
    if (activePoke == 0) {
      activePoke = 1;
    }
    else if (activePoke == 1) {
      activePoke = 0;
    }
    consecutive = 0;
    }
}

/**************************************************************************************************************************************************
                                                                                                Feeding functions
**************************************************************************************************************************************************/

void FED3::FeedLeft(int steps,int pulse, bool pixelsoff) {
  if (steps == 0) steps = doseLeftSteps;
  numMotorTurnsLeft = 0;
  //Run this loop repeatedly until statement below is false
  bool pelletDispensed = false;
  if (pelletDispensed == false) {
    pelletDispensed = RotateDiskLeft(steps);
  }

  if (pixelsoff==true){
    pixelsOff();
  }
    
    //If pellet is detected during or after this motion
  if (pelletDispensed == true) {
    // Immediately finish dispense and return to the loop
    ReleaseMotor();
    LeftDropTime = millis();
    retInterval = (millis() - LeftDropTime);
    LeftDeliverCount++;
    TotalDeliverCount++;
    if (pulse > 0){
      BNC (pulse, 1);  
    }
    Event = "LeftDeliver";
    
    LeftDropAvailable = true;
    UpdateDisplay();
    logdata();
    return;
  }
}

void FED3::FeedRight(int steps,int pulse, bool pixelsoff) {
  if (steps == 0) steps = doseRightSteps;
  numMotorTurnsRight = 0;
  //Run this loop repeatedly until statement below is false
  bool pelletDispensed = false;
  if (pelletDispensed == false) {
	    pelletDispensed = RotateDiskRight(steps);
  }

  if (pixelsoff==true){
    pixelsOff();
  }
    //If pellet is detected during or after this motion
  if (pelletDispensed == true) {    
    ReleaseMotor ();
    RightDropTime = millis();   
    retInterval = (millis() - RightDropTime);
    RightDeliverCount++;
    TotalDeliverCount++;
      // If pulse duration is specified, send pulse from BNC port
    if (pulse > 0){
      BNC (pulse, 1);  
    }
    Event = "RightDeliver";

      //calculate InterPelletInterval
    time_t nowTime = now();
    interPelletInterval = nowTime - lastPellet;  //calculate time in seconds since last pellet logged
    lastPellet  = nowTime;
    UpdateDisplay();
    logdata();
    RightDropAvailable = true;
    return;
  }
}

//////////////////////////
//jam functions no longer used for liquid, but left here for reference
///////////////////////////
/*minor movement to clear jam
bool FED3::MinorJam(){
	return RotateDisk(100);
}
//vibration movement to clear jam
bool FED3::VibrateJam() {
    DisplayJamClear();
	
	//simple debounce to ensure pellet is out for at least 250ms
	if (dispenseTimer_ms(250)) {
	  display.fillRect (5, 15, 120, 15, WHITE);  //erase the "Jam clear" text without clearing the entire screen by pasting a white box over it
	  return true;
	}	
	for (int i = 0; i < 30; i++) {
	  if (RotateDisk(120)) {
	    display.fillRect (5, 15, 120, 15, WHITE);  //erase the "Jam clear" text without clearing the entire screen by pasting a white box over it
	    return true;
	  }
	  if (RotateDisk(-60)) {
	    display.fillRect (5, 15, 120, 15, WHITE);  //erase the "Jam clear" text without clearing the entire screen by pasting a white box over it
	    return true;
	  }
	}
	return false;
}

//full rotation to clear jam
bool FED3::ClearJam() {
    DisplayJamClear();
	
	if (dispenseTimer_ms(250)) {
	  display.fillRect (5, 15, 120, 15, WHITE);  //erase the "Jam clear" text without clearing the entire screen by pasting a white box over it
	  return true;
	}
	
	for (int i = 0; i < 21 + random(0, 20); i++) {
	  if (RotateDisk(-i * 4)) {
	    display.fillRect (5, 15, 120, 15, WHITE);  //erase the "Jam clear" text without clearing the entire screen by pasting a white box over it
	    return true;
	  }
	}
	
	if (dispenseTimer_ms(250)) {
	  display.fillRect (5, 15, 120, 15, WHITE);  //erase the "Jam clear" text without clearing the entire screen by pasting a white box over it
	  return true;
	}
	
	for (int i = 0; i < 21 + random(0, 20); i++) {
	  if (RotateDisk(i * 4)) {
	    display.fillRect (5, 15, 120, 15, WHITE);  //erase the "Jam clear" text without clearing the entire screen by pasting a white box over it
	    return true;
	  }
	}
	
	return false;
}
*/
bool FED3::RotateDiskLeft(int steps) {
  digitalWrite (MOTOR_ENABLE_LEFT, HIGH);  //Enable left motor driver 
  stepperLeft.setSpeed(dispenseRPM);   // adjust to taste  
  int dir = (steps >= 0) ? 1 : -1; // determine direction based on sign of steps
  steps = abs(steps);
  stepperLeft.step(dir*steps); 
	ReleaseMotor ();
  Serial.println("RotateDiskLeft done");
	return true;
}

bool FED3::RotateDiskRight(int steps) {
  digitalWrite (MOTOR_ENABLE_RIGHT, HIGH);  //Enable right motor driver
  stepperRight.setSpeed(dispenseRPM);   // adjust to taste
  int dir = (steps >= 0) ? 1 : -1; // determine direction based on sign of steps
  steps = abs(steps);
  stepperRight.step(dir*steps);
	ReleaseMotor ();
	return true;
}

//helper function for lick sensor
void FED3::serviceLicks(){
  static uint16_t lastLick = 0; //last time a lick was detected
  uint16_t currentLick = cap.touched(); //current time
  uint16_t rise = currentLick & ~lastLick; //current time in ms
  if (rise & (1 << LEFT_LICK)) { //if left lick is detected
    LeftLickCount++;
    lickLeftFlag = true; //set left lick flag
    LeftDropAvailable = false; //reset left pellet well
    logLeftLick();
  }
  if (rise & (1 << RIGHT_LICK)) { //if right lick is detected
    RightLickCount++;
    lickRightFlag = true; //set right lick flag
    RightDropAvailable = false; //reset right pellet well
    logRightLick();
  }
  lastLick = currentLick; //update last lick time
  cap.touched(); //clear the interrupt flag
  lickIRQ = false; //reset lick interrupt flag
}

//Function for delaying between motor movements, but also ending this delay if a pellet is detected
bool FED3::dispenseTimer_ms(int ms) {
  for (int i = 1; i < ms; i++) {
    for (int j = 0; j < 10; j++) {
  	  delayMicroseconds(100);		
	}
  }
  return false;
}

//Timeout function

void FED3::Timeout(int seconds, bool reset, bool whitenoise) {
  int timeoutStart = millis();

  while ((millis() - timeoutStart) < (static_cast<unsigned long>(seconds)*1000UL)) {
    if (whitenoise) {
      int freq = random(50,250);
      tone(BUZZER, freq, 10);
      delay (10);
    }

    if (digitalRead(LEFT_POKE) == LOW) {             //If left poke is triggered
      if (reset) {
        timeoutStart = millis();
      }
      leftPokeTime = millis();
      if (countAllPokes) {
        LeftCount ++;
      }

      leftInterval = 0.0;      
      while (digitalRead (LEFT_POKE) == LOW) {  //Hang here until poke is clear
        if (whitenoise) {
          int freq = random(50,250);
          tone(BUZZER, freq, 10);
        }
      }  

      leftInterval = (millis() - leftPokeTime);
      Event = "LeftinTimeOut";
      logdata();
    }

    if (digitalRead(RIGHT_POKE) == LOW) {        //If right poke is triggered
      if (reset) {
        timeoutStart = millis();
      }
      if (countAllPokes) {
        RightCount ++;
      }
      rightPokeTime = millis();

      rightInterval = 0.0;  
      while (digitalRead (LEFT_POKE) == LOW) { //Hang here until poke is clear
        if (whitenoise) {
          int freq = random(50,250);
          tone(BUZZER, freq, 10);
        }
      }   
      rightInterval = (millis() - rightPokeTime);
      UpdateDisplay();
      Event = "RightinTimeout";
      logdata();

    }
  }
  display.fillRect (5, 20, 100, 25, WHITE);  //erase the data on screen without clearing the entire screen by pasting a white box over it
  UpdateDisplay();
  Left = false;
  Right = false;
}

/**************************************************************************************************************************************************
                                                                                       Audio and neopixel stimuli
**************************************************************************************************************************************************/
void FED3::ConditionedStimulus(int duration) {
  tone (BUZZER, 4000, duration);
  pixelsOn(0,0,10,0);  //blue light for all
}

void FED3::Click() {
  tone (BUZZER, 800, 8);
}

void FED3::Tone(int freq, int duration){
  tone (BUZZER, freq, duration);
}

void FED3::stopTone(){
  noTone (BUZZER);
}


void FED3::Noise(int duration) {
  // White noise to signal errors
  for (int i = 0; i < duration/50; i++) {
    tone (BUZZER, random(50, 250), 50);
    delay(duration/50);
  }
}

//Turn all pixels on to a specific color
void FED3::pixelsOn(int R, int G, int B, int W) {
  digitalWrite (MOTOR_ENABLE_LEFT, HIGH);
  digitalWrite (MOTOR_ENABLE_RIGHT, HIGH);  //ENABLE motor driver
  delay(2); //let things settle
  for (uint16_t i = 0; i < 8; i++) {
    strip.setPixelColor(i, R, G, B, W);
    strip.show();
  }
}

//Turn all pixels off
void FED3::pixelsOff() {
  digitalWrite (MOTOR_ENABLE_LEFT, HIGH);
  digitalWrite (MOTOR_ENABLE_RIGHT, HIGH);
  delay (2); //let things settle
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, 0,0,0,0);
    strip.show();   
  }
  digitalWrite (MOTOR_ENABLE_LEFT, LOW);
  digitalWrite (MOTOR_ENABLE_RIGHT, LOW);
  //disable motor driver and neopixels
}

//colorWipe does a color wipe from left to right
void FED3::colorWipe(uint32_t c, uint8_t wait) {
  digitalWrite (MOTOR_ENABLE_LEFT, HIGH);
  digitalWrite (MOTOR_ENABLE_RIGHT, HIGH);
  delay(2); //let things settle
  for (uint16_t i = 0; i < 8; i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
  digitalWrite (MOTOR_ENABLE_LEFT, LOW);
  digitalWrite (MOTOR_ENABLE_RIGHT, LOW);
  //disable motor driver and neopixels
  delay(2); //let things settle
}

// Visual tracking stimulus - left-most pixel on strip
void FED3::leftPixel(int R, int G, int B, int W) {
  digitalWrite (MOTOR_ENABLE_LEFT, HIGH);
  delay(2); //let things settle
  strip.setPixelColor(0, R, G, B, W);
  strip.show();
//   delay(2); //let things settle
}

// Visual tracking stimulus - left-most pixel on strip
void FED3::rightPixel(int R, int G, int B, int W) {
  digitalWrite (MOTOR_ENABLE_RIGHT, HIGH);
  delay(2); //let things settle
  strip.setPixelColor(7, R, G, B, W);
  strip.show();
//   delay(2); //let things settle
}

// Visual tracking stimulus - left poke pixel
void FED3::leftPokePixel(int R, int G, int B, int W) {
  digitalWrite (MOTOR_ENABLE_LEFT, HIGH);
  delay(2); //let things settle
  strip.setPixelColor(9, R, G, B, W);
  strip.show();
//   delay(2); //let things settle
}

// Visual tracking stimulus - right poke pixel
void FED3::rightPokePixel(int R, int G, int B, int W) {
  digitalWrite (MOTOR_ENABLE_RIGHT, HIGH);
  delay(2); //let things settle
  strip.setPixelColor(8, R, G, B, W);
  strip.show();
  //delay(2); //let things settle
}

//Short helper function for blinking LEDs and BNC out port
void FED3::Blink(byte PIN, byte DELAY_MS, byte loops) {
  for (byte i = 0; i < loops; i++)  {
    digitalWrite(PIN, HIGH);
    delay(DELAY_MS);
    digitalWrite(PIN, LOW);
    delay(DELAY_MS);
  }
}

//Simple function for sending square wave pulses to the BNC port
void FED3::BNC(int DELAY_MS, int loops) {
  for (int i = 0; i < loops; i++)  {
    digitalWrite(BNC_OUT, HIGH);
    digitalWrite(GREEN_LED, HIGH);
    delay(DELAY_MS);
    digitalWrite(BNC_OUT, LOW);
    digitalWrite(GREEN_LED, LOW);
    delay(DELAY_MS);
  }
}

//More advanced function for controlling pulse width and frequency for the BNC port
void FED3::pulseGenerator(int pulse_width, int frequency, int repetitions){  // freq in Hz, width in ms, loops in number of times
  for (byte j = 0; j < repetitions; j++) {
    digitalWrite(BNC_OUT, HIGH);
    digitalWrite(GREEN_LED, HIGH);
    delay(pulse_width);  //pulse high for width
    digitalWrite(BNC_OUT, LOW);
    digitalWrite(GREEN_LED, LOW);
    long temp_delay = (1000 / frequency) - pulse_width;
    if (temp_delay < 0) temp_delay = 0;  //if temp delay <0 because parameters are set wrong, set it to 0 so FED3 doesn't crash O_o
    delay(temp_delay); //pin low
  }
}

void FED3::ReadBNC(bool blinkGreen){
    pinMode(BNC_OUT, INPUT_PULLDOWN);
    BNCinput=false;
    if (digitalRead(BNC_OUT) == HIGH)
    {
      delay (1);
      if (digitalRead(BNC_OUT) == HIGH)
      {
        if (blinkGreen == true)
        {
          digitalWrite(GREEN_LED, HIGH);
          delay (25);
          digitalWrite(GREEN_LED, LOW);
        }
        BNCinput=true;
      }
    }
}

/**************************************************************************************************************************************************
                                                                                               Display functions
**************************************************************************************************************************************************/
void FED3::UpdateDisplay() {
  //Box around data area of screen
  display.drawRect (5, 45, 158, 70, BLACK);
  
  display.setCursor(5, 15);
  display.print("FED:");
  display.println(FED);
  display.setCursor(6, 15);  // this doubling is a way to do bold type
  display.print("FED:");
  display.fillRect (6, 20, 200, 22, WHITE);  //erase text under battery row without clearing the entire screen
  display.fillRect (35, 46, 120, 68, WHITE);  //erase the pellet data on screen without clearing the entire screen 
  display.setCursor(5, 36); //display which sketch is running
  
  //write the first 8 characters of sessiontype:
  display.print(sessiontype.charAt(0));
  display.print(sessiontype.charAt(1));
  display.print(sessiontype.charAt(2));
  display.print(sessiontype.charAt(3));
  display.print(sessiontype.charAt(4));
  display.print(sessiontype.charAt(5));
  display.print(sessiontype.charAt(6));
  display.print(sessiontype.charAt(7));

  display.setCursor(35, 65);
  display.print("LeftLick : ");
  display.setCursor(120, 65);
  display.print(LeftLickCount);

  display.setCursor(35, 85);
  display.print("RightLick: ");
  display.setCursor(120, 85);
  display.print(RightLickCount);
  
  display.setCursor(35, 105);
  display.print("TotalDeli:");
  display.setCursor(120, 105);
  display.print(TotalDeliverCount);

  if (DisplayTimed==true) {  //If it's a timed Feeding Session
    DisplayTimedFeeding();
  }
  
  DisplayBattery();
  DisplayDateTime();
  DisplayIndicators();
  display.refresh();
}

void FED3::DisplayDateTime(){
  // Print date and time at bottom of the screen
  time_t nowTime = now();
  display.setCursor(0, 135);
  display.fillRect (0, 123, 200, 60, WHITE);
  display.print(month(nowTime));
  display.print("/");
  display.print(day(nowTime));
  display.print("/");
  display.print(year(nowTime));
  display.print("      ");
  if (hour(nowTime) < 10)
    display.print('0');      // Trick to add leading zero for formatting
  display.print(hour(nowTime));
  display.print(":");
  if (minute(nowTime) < 10)
    display.print('0');      // Trick to add leading zero for formatting
  display.print(minute(nowTime));
}

void FED3::DisplayIndicators(){
  // Pellet circle
  display.fillCircle(25, 99, 5, WHITE); //pellet
  display.drawCircle(25, 99, 5, BLACK);

  //Poke indicators
  if (DisplayPokes == 1) { //only make active poke indicators if DisplayPokes==1
    //Active poke indicator triangles
    if (activePoke == 0) {
      display.fillTriangle (20, 55, 26, 59, 20, 63, WHITE);
      display.fillTriangle (20, 75, 26, 79, 20, 83, BLACK);
    }
    if (activePoke == 1) {
      display.fillTriangle (20, 75, 26, 79, 20, 83, WHITE);
      display.fillTriangle (20, 55, 26, 59, 20, 63, BLACK);
    }
  }
}

void FED3::DisplayBattery(){
  //  Battery graphic showing bars indicating voltage levels
  if ((numMotorTurnsLeft + numMotorTurnsRight) == 0) {
    display.fillRect (117, 2, 40, 16, WHITE);
    display.drawRect (116, 1, 42, 18, BLACK);
    display.drawRect (157, 6, 6, 8, BLACK);
  }
  //4 bars
  if (measuredvbat > 3.85 && (numMotorTurnsLeft + numMotorTurnsRight) == 0) {
    display.fillRect (120, 4, 7, 12, BLACK);
    display.fillRect (129, 4, 7, 12, BLACK);
    display.fillRect (138, 4, 7, 12, BLACK);
    display.fillRect (147, 4, 7, 12, BLACK);
  }

  //3 bars
  else if (measuredvbat > 3.7 && (numMotorTurnsLeft + numMotorTurnsRight) == 0) {
    display.fillRect (119, 3, 26, 13, WHITE);
    display.fillRect (120, 4, 7, 12, BLACK);
    display.fillRect (129, 4, 7, 12, BLACK);
    display.fillRect (138, 4, 7, 12, BLACK);
  }

  //2 bars
  else if (measuredvbat > 3.55 && (numMotorTurnsLeft + numMotorTurnsRight) == 0) {
    display.fillRect (119, 3, 26, 13, WHITE);
    display.fillRect (120, 4, 7, 12, BLACK);
    display.fillRect (129, 4, 7, 12, BLACK);
  }

  //1 bar
  else if ((numMotorTurnsLeft + numMotorTurnsRight) == 0) {
    display.fillRect (119, 3, 26, 13, WHITE);
    display.fillRect (120, 4, 7, 12, BLACK);
  }
  
  //display voltage
  display.setTextSize(2);
  display.setFont(&Org_01);

  display.fillRect (86, 0, 28, 12, WHITE);
  display.setCursor(87, 10);
  display.print(measuredvbat, 1);
  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);
  
  //display temp/humidity sensor indicator if present
  if (tempSensor == true){
    display.setTextSize(1);
    display.setFont(&Org_01);
    display.setCursor(89, 18);
    display.print("TH");
    display.setFont(&FreeSans9pt7b);
    display.setTextSize(1);
  }
}

//Display "Check SD Card!" if there is a card error
void FED3::DisplaySDError() {
  display.clearDisplay();
  display.setCursor(20, 40);
  display.println("   Check");
  display.setCursor(10, 60);
  display.println("  SD Card!");
  display.refresh();
}

//Display text when FED is clearing a jam
void FED3::DisplayJamClear() {
  display.fillRect (6, 20, 200, 22, WHITE);  //erase the data on screen without clearing the entire screen by pasting a white box over it
  display.setCursor(6, 36);
  display.print("Clearing jam");
  display.refresh();
}

//Display text when FED is clearing a jam
void FED3::DisplayJammed() {
  display.clearDisplay();
  display.fillRect (6, 20, 200, 22, WHITE);  //erase the data on screen without clearing the entire screen by pasting a white box over it
  display.setCursor(6, 36);
  display.print("JAMMED...");
  display.print("PLEASE CHECK");
  display.refresh();
  ReleaseMotor();
  delay (2); //let things settle 
  DisplayJammed();
}

//Display pellet retrieval interval
void FED3::DisplayRetrievalInt() {
  display.fillRect (85, 22, 70, 15, WHITE); 
  display.setCursor(90, 36);
  if (retInterval<59000){
    display.print (retInterval);
    display.print ("ms");
  }
  display.refresh();
}

//Display left poke duration
void FED3::DisplayLeftInt() {
  display.fillRect (85, 22, 70, 15, WHITE);  
  display.setCursor(90, 36);
  if (leftInterval<10000){
    display.print (leftInterval);
    display.print ("ms");
  }
  display.refresh();
}

//Display right poke duration
void FED3::DisplayRightInt() {
  display.fillRect (85, 22, 70, 15, WHITE);  
  display.setCursor(90, 36);
  if (rightInterval<10000){
    display.print (rightInterval);
    display.print ("ms");
  }
  display.refresh();
}

void FED3::StartScreen(){
  if (ClassicFED3==false){
    display.setTextSize(3);
    display.setTextColor(BLACK);
    display.clearDisplay();
    display.setCursor(15, 55);
    display.print("FED3");
      
    //print filename on screen
    display.setTextSize(1);
    display.setCursor(2, 138);
    display.print(filename);

    //Display FED version number at startup
    display.setCursor(2, 120);
    display.print("v: ");
    display.print(VER);
    display.print("_");
    display.print(sessiontype.charAt(0));
    display.print(sessiontype.charAt(1));
    display.print(sessiontype.charAt(2));
    display.print(sessiontype.charAt(3));
    display.print(sessiontype.charAt(4));
    display.print(sessiontype.charAt(5));
    display.print(sessiontype.charAt(6));
    display.print(sessiontype.charAt(7));
    display.refresh();
    DisplayMouse();
  }
}

void FED3::DisplayTimedFeeding(){
  display.setCursor(35, 65);
  display.print (timedStart);
  display.print (":00 to ");
  display.print (timedEnd);
  display.print (":00");
}

void FED3::DisplayMinPoke(){
  display.setCursor(115, 65);
  display.print ((minPokeTime/1000.0),1);
  display.print ("s");
  display.refresh();
}

void FED3::DisplayNoProgram(){
  display.clearDisplay();
  display.setCursor(15, 45);
  display.print ("No program");
  display.setCursor(16, 45);
  display.print ("No program");
  display.setCursor(15, 65);
  display.print ("resetting FED3...");
  display.refresh();
  for (int i = 0; i < 5; i++) {
    colorWipe(strip.Color(5, 0, 0), 25); // RED
    delay (20);
    colorWipe(strip.Color(0, 0, 0), 25); // clear
    delay (40);
  }
  teensyReset();
}

void FED3::DisplayMouse() {
  static uint32_t bothLowSince = 0;     // remembers when both pokes first went LOW
  //Draw animated mouse...
  for (int i = -50; i < 200; i += 15) {
    display.fillRoundRect (i + 25, 82, 15, 10, 6, BLACK);    //head
    display.fillRoundRect (i + 22, 80, 8, 5, 3, BLACK);      //ear
    display.fillRoundRect (i + 30, 84, 1, 1, 1, WHITE);      //eye
    //movement of the mouse
    if ((i / 10) % 2 == 0) {
      display.fillRoundRect (i, 84, 32, 17, 10, BLACK);      //body
      display.drawFastHLine(i - 8, 85, 18, BLACK);           //tail
      display.drawFastHLine(i - 8, 86, 18, BLACK);
      display.drawFastHLine(i - 14, 84, 8, BLACK);
      display.drawFastHLine(i - 14, 85, 8, BLACK);
      display.fillRoundRect (i + 22, 99, 8, 4, 3, BLACK);    //front foot
      display.fillRoundRect (i , 97, 8, 6, 3, BLACK);        //back foot
    }
    else {
      display.fillRoundRect (i + 2, 82, 30, 17, 10, BLACK);  //body
      display.drawFastHLine(i - 6, 91, 18, BLACK);           //tail
      display.drawFastHLine(i - 6, 90, 18, BLACK);
      display.drawFastHLine(i - 12, 92, 8, BLACK);
      display.drawFastHLine(i - 12, 91, 8, BLACK);
      display.fillRoundRect (i + 15, 99, 8, 4, 3, BLACK);    //foot
      display.fillRoundRect (i + 8, 97, 8, 6, 3, BLACK);     //back foot
    }
    display.refresh();
    delay (80);
    display.fillRect (i-25, 73, 95, 33, WHITE);
    previousFEDmode = FEDmode;
    previousFED = FED;
    
    // If one poke is pushed change mode
    if (FED3Menu == true or ClassicFED3 == true or psygene){
      if (digitalRead (LEFT_POKE) == LOW || digitalRead (RIGHT_POKE) == LOW) SelectMode();
    }
    
    // If both pokes are pushed edit device number
    if (digitalRead(LEFT_POKE)  == LOW && digitalRead(RIGHT_POKE) == LOW) {
      if (bothLowSince == 0) bothLowSince = millis();          // start timer
      if (millis() - bothLowSince > 1500) {                    // 1.5-s hold
        tone(BUZZER, 1000, 200);  delay(400);
        tone(BUZZER, 1000, 500);  delay(200);
        tone(BUZZER, 3000, 600);
        colorWipe(strip.Color(2, 2, 2), 40);                 // white flash
        colorWipe(strip.Color(0, 0, 0), 20);                 // off

        SetFED = true;
        SetDeviceNumber();
        return;                                              // leave loop
      }
    } else {
      bothLowSince = 0;                                        // any release aborts
    }
  }
}

/**************************************************************************************************************************************************
                                                                                               SD Logging functions
**************************************************************************************************************************************************/
// Create new files on uSD for FED3 settings
void FED3::CreateFile() {
  Serial.println(F("→ CreateFile():SD. begin"));
  digitalWrite (MOTOR_ENABLE, LOW);  //Disable motor driver and neopixel
  // see if the card is present and can be initialized:
  #if defined(__IMXRT1062__)  // Teensy 4.0/4.1专用
  if (!SD.begin(SdioConfig(FIFO_SDIO))) {
    Serial.println("SD init failed on Teensy!");
    error(2);
  }
  #else
  if (!SD.begin(SdioConfig(FIFO_SDIO))) {
    Serial.print("SD init error: ");
    Serial.println(SD.sdErrorCode());
    Serial.println(SD.sdErrorData());
    error(2);
  } else {
    Serial.println(F("sd.BEGIN OK"));
  }
  #endif
       // wait until card is really idle

  // create files if they dont exist and grab device name and ratio
  if (SD.exists("DeviceNumber.csv")) {
    configfile = SD.open("DeviceNumber.csv", FILE_READ);
    FED = configfile.parseInt();           // reuse the number that was stored last time
    configfile.close();
  } else {
    FED = 1;                               // or whatever default you prefer
    configfile = SD.open("DeviceNumber.csv", FILE_WRITE); // first-time creation
    configfile.println(FED);
    configfile.close();
  }

  ratiofile = SD.open("FEDmode.csv", FILE_WRITE);
  if (!ratiofile) {
    Serial.println("Failed to open FEDmode.csv");
    error(3);
  }
  ratiofile.close();    // wait until card is really idle
  ratiofile = SD.open("FEDmode.csv", FILE_READ);
  FEDmode = ratiofile.parseInt();
  ratiofile.close();

  startfile = SD.open("start.csv", FILE_WRITE);
  if (!startfile) {
    Serial.println("Failed to open start.csv");
    error(3);
  }
  startfile.close();    // wait until card is really idle
  startfile = SD.open("start.csv", FILE_READ);
  timedStart = startfile.parseInt();
  startfile.close();

  stopfile = SD.open("stop.csv", FILE_WRITE);
  if (!stopfile) {
    Serial.println("Failed to open stop.csv");
    error(3);
  }
  stopfile.close();    // wait until card is really idle
  stopfile = SD.open("stop.csv", FILE_READ);
  timedEnd = stopfile.parseInt();
  stopfile.close();

  // Name filename in format F###_MMDDYYNN, where MM is month, DD is day, YY is year, and NN is an incrementing number for the number of files initialized each day
  strcpy(filename, "FED_____________.CSV");  // placeholder filename
  getFilename(filename);
}

//Create a new datafile
void FED3::CreateDataFile () {
  Serial.println(F("→ CreateDataFile():SD.open"));
  digitalWrite (MOTOR_ENABLE, LOW);  //Disable motor driver and neopixel
  getFilename(filename);
  Serial.print(F("filename = "));
  Serial.println(filename);
  logfile = SD.open(filename, FILE_WRITE);
  if (!logfile) {
    Serial.print("Failed to open: ");
    Serial.println(filename);
    Serial.print("Error: ");
    Serial.println(SD.sdErrorCode());
    error(3);
  }
  if (!logfile) SD.errorPrint(&Serial);
  Serial.println(logfile ? F("    logfile OK") : F("    logfile FAIL"));
  if ( ! logfile ) {
    error(3);
  }
}

//Write the header to the datafile
void FED3::writeHeader() {
  digitalWrite (MOTOR_ENABLE, LOW);  //Disable motor driver and neopixel
  // Write data header to file of microSD card

  if ((sessiontype == "Bandit") or (sessiontype == "Bandit80") or (sessiontype == "Bandit100")){
    if (tempSensor == false) {
      logfile.println("MM:DD:YYYY hh:mm:ss:ms,Library_Version,Session_type,Device_Number,Battery_Voltage,Left_Motor_Turns,Right_Motor_Turns,PelletsToSwitch,Prob_left,Prob_right,Event,High_prob_poke,Left_Poke_Count,Right_Poke_Count,Left_Lick_Count,Right_Lick_Count,Left_Deliver_Count,Right_Deliver_Count,Block_Pellet_Count,Retrieval_Time,InterPelletInterval,Poke_Time");
    }
    else if (tempSensor == true) {
      logfile.println("MM:DD:YYYY hh:mm:ss:ms,Temp,Humidity,Library_Version,Session_type,Device_Number,Battery_Voltage,Left_Motor_Turns,Right_Motor_Turns,PelletsToSwitch,Prob_left,Prob_right,Event,High_prob_poke,Left_Poke_Count,Right_Poke_Count,Left_Lick_Count,Right_Lick_Count,Left_Deliver_Count,Right_Deliver_Count,Block_Pellet_Count,Retrieval_Time,InterPelletInterval,Poke_Time");
    }
  }

  else {
    if (tempSensor == false){
      logfile.println("MM:DD:YYYY hh:mm:ss:ms,Library_Version,Session_type,Device_Number,Battery_Voltage,Left_Motor_Turns,Right_Motor_Turns,FR,Event,Active_Poke,Left_Poke_Count,Right_Poke_Count,Left_Lick_Count,Right_Lick_Count,Left_Deliver_Count,Right_Deliver_Count,Block_Pellet_Count,Retrieval_Time,InterPelletInterval,Poke_Time");
    }
    if (tempSensor == true){
      logfile.println("MM:DD:YYYY hh:mm:ss:ms,Temp,Humidity,Library_Version,Session_type,Device_Number,Battery_Voltage,Left_Motor_Turns,Right_Motor_Turns,FR,Event,Active_Poke,Left_Poke_Count,Right_Poke_Count,Left_Lick_Count,Right_Lick_Count,Left_Deliver_Count,Right_Deliver_Count,Block_Pellet_Count,Retrieval_Time,InterPelletInterval,Poke_Time");
    }
  }


  logfile.close();
}

//write a configfile (this contains the FED device number)
void FED3::writeConfigFile() {
  digitalWrite (MOTOR_ENABLE, LOW);  //Disable motor driver and neopixel
  configfile = SD.open("DeviceNumber.csv", FILE_WRITE);
  configfile.seek(0);
  configfile.println(FED);
  configfile.flush();
  configfile.close();
}

//Write to SD card
void FED3::logdata() {
  bool isDeliver = (Event == "LeftDeliver" || Event == "RightDeliver");
  if (EnableSleep==true){
    digitalWrite (MOTOR_ENABLE, LOW);  //Disable motor driver and neopixel
  }
  SD.begin(SdioConfig(FIFO_SDIO));
  
  //fix filename (the .CSV extension can become corrupted) and open file
  filename[16] = '.';
  filename[17] = 'C';
  filename[18] = 'S';
  filename[19] = 'V';
  logfile = SD.open(filename, FILE_WRITE);

  //if FED3 cannot open file put SD card icon on screen 
  display.fillRect (68, 1, 15, 22, WHITE); //clear a space
  if ( ! logfile ) {
  
    //draw SD card icon
    display.drawRect (70, 2, 11, 14, BLACK);
    display.drawRect (69, 6, 2, 10, BLACK);
    display.fillRect (70, 7, 4, 8, WHITE);
    display.drawRect (72, 4, 1, 3, BLACK);
    display.drawRect (74, 4, 1, 3, BLACK);
    display.drawRect (76, 4, 1, 3, BLACK);
    display.drawRect (78, 4, 1, 3, BLACK);
    //exclamation point
    display.fillRect (72, 6, 6, 16, WHITE);
    display.setCursor(74, 16);
    display.setTextSize(2);
    display.setFont(&Org_01);
    display.print("!");
    display.setFont(&FreeSans9pt7b);
    display.setTextSize(1);
  }
  
  /////////////////////////////////
  // Log data and time 
  /////////////////////////////////
  time_t nowTime = now();
  unsigned long msPart = millis() % 1000; // Get milliseconds part of the time
  logfile.print(month(nowTime));
  logfile.print("/");
  logfile.print(day(nowTime));
  logfile.print("/");
  logfile.print(year(nowTime));
  logfile.print(" ");
  logfile.print(hour(nowTime));
  logfile.print(":");
  if (minute(nowTime) < 10)
    logfile.print('0');      // Trick to add leading zero for formatting
  logfile.print(minute(nowTime));
  logfile.print(":");
  if (second(nowTime) < 10)
    logfile.print('0');      // Trick to add leading zero for formatting
  logfile.print(second(nowTime));
  logfile.print(":");
  if (msPart < 100) logfile.print('0');
  if (msPart <  10) logfile.print('0');
  logfile.print(msPart);
  logfile.print(",");
    
  /////////////////////////////////
  // Log temp and humidity
  /////////////////////////////////
  if (tempSensor == true){
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
    logfile.print (temp.temperature);
    logfile.print(",");
    logfile.print (humidity.relative_humidity);
    logfile.print(",");
  }

  /////////////////////////////////
  // Log library version and Sketch identifier text
  /////////////////////////////////
  logfile.print(VER); // Print library version
  logfile.print(",");
  
  /////////////////////////////////
  // Log Trial Info
  /////////////////////////////////
  logfile.print(sessiontype);  //print Sketch identifier
  logfile.print(",");
  
  /////////////////////////////////
  // Log FED device number
  /////////////////////////////////
  logfile.print(FED); // 
  logfile.print(",");

  /////////////////////////////////
  // Log battery voltage
  /////////////////////////////////
  ReadBatteryLevel();
  logfile.print(measuredvbat); // 
  logfile.print(",");

  /////////////////////////////////
  // Log motor turns
  /////////////////////////////////
  if (!isDeliver) { // if it's not a pellet delivery event
    logfile.print(sqrt (-1)); // print NaN if it's not a pellet Event
    logfile.print(",");
    logfile.print(sqrt (-1)); // print NaN if it's not a pellet Event
    logfile.print(",");
  }
  else {
    logfile.print(numMotorTurnsLeft+1); // Print the number of attempts to dispense a pellet
    logfile.print(",");
    logfile.print(numMotorTurnsRight+1);
    logfile.print(",");
  }

  /////////////////////////////////////////////////////////////
  // Log FR ratio (or pellets to switch block in bandit task)

  if ((sessiontype == "Bandit") or (sessiontype == "Bandit80") or (sessiontype == "Bandit100")) {
    logfile.print(pelletsToSwitch);
    logfile.print(",");
    logfile.print(prob_left);
    logfile.print(",");
    logfile.print(prob_right);
    logfile.print(",");
  }
  else {
    logfile.print(FR);
    logfile.print(",");
  }

  /////////////////////////////////
  // Log event type (pellet, right, left)
  /////////////////////////////////
  logfile.print(Event); 
  logfile.print(",");

  /////////////////////////////////
  // Log Active poke side (left, right)
  /////////////////////////////////
  if ((sessiontype == "Bandit") or (sessiontype == "Bandit80") or (sessiontype == "Bandit100")) {
    if (prob_left > prob_right) logfile.print("Left");
    else if (prob_left < prob_right) logfile.print("Right");
    else if (prob_left == prob_right) logfile.print("nan");
  }
  
  else {
    if (activePoke == 0)  logfile.print("Right"); //
    if (activePoke == 1)  logfile.print("Left"); //
  }

  logfile.print(",");


  /////////////////////////////////
  // Log data (leftCount, RightCount, Pellets)
  /////////////////////////////////
  logfile.print(LeftCount); // Print Left poke count
  logfile.print(",");
    
  logfile.print(RightCount); // Print Right poke count
  logfile.print(",");

  logfile.print(LeftLickCount); // Print Left/right lick count
  logfile.print(",");
  logfile.print(RightLickCount);
  logfile.print(",");

  logfile.print(LeftDeliverCount); // print left drop counts
  logfile.print(",");
  logfile.print(RightDeliverCount); // print right drop counts
  logfile.print(",");

  logfile.print(BlockPelletCount); // print Block Pellet counts
  logfile.print(",");

  

  /////////////////////////////////
  // Log pellet retrieval interval
  /////////////////////////////////
  if (!isDeliver) { // if it's not a pellet delivery event
    logfile.print(sqrt (-1)); // print NaN if it's not a pellet Event
  }
  else if (retInterval < 60000 ) {  // only log retrieval intervals below 1 minute (FED should not record any longer than this)
    logfile.print(retInterval/1000.000); // print interval between pellet dispensing and being taken
  }
  else if (retInterval >= 60000) {
    logfile.print("Timed_out"); // print "Timed_out" if retreival interval is >60s
  }
  else {
    logfile.print("Error"); // print error if value is < 0 (this shouldn't ever happen)
  }
  logfile.print(",");
  
  
  /////////////////////////////////
  // Inter-Pellet-Interval
  /////////////////////////////////
  if ((!isDeliver) or (TotalDeliverCount < 2)){
    logfile.print(sqrt (-1)); // print NaN if it's not a pellet Event
  }
  else {
    logfile.print (interPelletInterval);
  }
  logfile.print(",");
      
  /////////////////////////////////
  // Poke duration
  /////////////////////////////////
  if (isDeliver){
    logfile.println(sqrt (-1)); // print NaN 
  }

  else if ((Event == "Left") or (Event == "LeftShort") or (Event == "LeftWithPellet") or (Event == "LeftinTimeout") or (Event == "LeftDuringDispense")) {  // 
    logfile.println(leftInterval/1000.000); // print left poke timing
  }

  else if ((Event == "Right") or (Event == "RightShort") or (Event == "RightWithPellet") or (Event == "RightinTimeout") or (Event == "RightDuringDispense")) {  // 
    logfile.println(rightInterval/1000.000); // print left poke timing
  }
  
  else {
    logfile.println(sqrt (-1)); // print NaN 
  }

  /////////////////////////////////
  // logfile.flush write to the SD card
  /////////////////////////////////
  Blink(GREEN_LED, 25, 2);
  logfile.flush();
  logfile.close();
}



// If any errors are detected with the SD card upon boot this function
// will blink both LEDs on the Feather M0, turn the NeoPixel into red wipe pattern,
// and display "Check SD Card" on the screen
void FED3::error(uint8_t errno) {
  if (suppressSDerrors == false){
    DisplaySDError();
    while (1) {
      uint8_t i;
      for (i = 0; i < errno; i++) {
        Blink(GREEN_LED, 25, 2);
        colorWipe(strip.Color(5, 0, 0), 25); // RED
      }
      for (i = errno; i < 10; i++) {
        colorWipe(strip.Color(0, 0, 0), 25); // clear
      }
    }
  }
}

// This function creates a unique filename for each file that
// starts with the letters: "FED_" 
// then the date in MMDDYY followed by "_"
// then an incrementing number for each new file created on the same date
void FED3::getFilename(char *filename) {
   time_t nowTime = now();

  filename[3] = FED / 100 + '0';
  filename[4] = FED / 10 + '0';
  filename[5] = FED % 10 + '0';
  filename[7] = month(nowTime) / 10 + '0';
  filename[8] = month(nowTime) % 10 + '0';
  filename[9] = day(nowTime) / 10 + '0';
  filename[10] = day(nowTime) % 10 + '0';
  filename[11] = (year(nowTime) - 2000) / 10 + '0';
  filename[12] = (year(nowTime) - 2000) % 10 + '0';
  filename[16] = '.';
  filename[17] = 'C';
  filename[18] = 'S';
  filename[19] = 'V';

  for (uint8_t i = 0; i < 100; i++) {
    filename[14] = '0' + i / 10;
    filename[15] = '0' + i % 10;

    if (SD.exists(filename)) {
      // Open the file to check its length
      FsFile file = SD.open(filename, FILE_READ);
      if (file) {
        int lineCount = 0;
        while (file.available()) {
          if (file.read() == '\n') {
            lineCount++;
          }
        }
        file.close();

        // If the file has less than 3 lines, delete it
        if (lineCount < 3) {
          SD.remove(filename);
          break;
        }
      } else {
        // If the file cannot be opened, log an error
        Serial.println("Error opening file for reading.");
      }
    } else {
      // If the file does not exist, use this filename
      break;
    }
  }
  return;
}

/**************************************************************************************************************************************************
                                                                                               Change device number
**************************************************************************************************************************************************/
// Change device number
void FED3::SetDeviceNumber() {
  // This code is activated when both pokes are pressed simultaneously from the 
  //start screen, allowing the user to set the device # of the FED on the device
  while (SetFED == true) {
    //adjust FED device number
    display.fillRect (0, 0, 200, 80, WHITE);
    display.setCursor(5, 46);
    display.println("Set Device Number");
    display.fillRect (36, 122, 180, 28, WHITE);
    delay (100);
    display.refresh();

    display.setCursor(38, 138);
    if (FED < 100 && FED >= 10) {
      display.print ("0");
    }
    if (FED < 10) {
      display.print ("00");
    }
    display.print (FED);

    delay (100);
    display.refresh();

    if (digitalRead(RIGHT_POKE) == LOW) {
      FED += 1;
      Click();
      EndTime = millis();
      if (FED > 700) {
        FED = 700;
      }
    }

    if (digitalRead(LEFT_POKE) == LOW) {
      FED -= 1;
      Click();
      EndTime = millis();
      if (FED < 1) {
        FED = 0;
      }
    }
    if (millis() - EndTime > 3000) {  // if 3 seconds passes confirm device #
      SetFED = false;
      display.setCursor(5, 70);
      display.println("...Set!");
      display.refresh();
      delay (1000);
      EndTime = millis();
      display.clearDisplay();
      display.refresh();
            
      ///////////////////////////////////
      //////////  ADJUST CLOCK //////////
      while (millis() - EndTime < 3000) { 
        SetClock();
        delay (10);
      }

      display.setCursor(5, 105);
      display.println("...Clock is set!");
      display.refresh();
      delay (1000);

      ///////////////////////////////////
      
      while (setTimed == true) {
        // set timed feeding start and stop
        display.fillRect (5, 56, 120, 18, WHITE);
        delay (200);
        display.refresh();

        display.fillRect (0, 0, 200, 80, WHITE);
        display.setCursor(5, 46);
        display.println("Set Timed Feeding");
        display.setCursor(15, 70);
        display.print(timedStart);
        display.print(":00 - ");
        display.print(timedEnd);
        display.print(":00");
        delay (50);
        display.refresh();

        if (digitalRead(LEFT_POKE) == LOW) {
          timedStart += 1;
          EndTime = millis();
          if (timedStart > 24) {
            timedStart = 0;
          }
          if (timedStart > timedEnd) {
            timedEnd = timedStart + 1;
          }
        }

        if (digitalRead(RIGHT_POKE) == LOW) {
          timedEnd += 1;
          EndTime = millis();
          if (timedEnd > 24) {
            timedEnd = 0;
          }
          if (timedStart > timedEnd) {
            timedStart = timedEnd - 1;
          }
        }
        if (millis() - EndTime > 3000) {  // if 3 seconds passes confirm time settings
          setTimed = false;
          display.setCursor(5, 95);
          display.println("...Timing set!");
          delay (1000);
          display.refresh();
        }
      }
      writeFEDmode();
      writeConfigFile();
      teensyReset();     // processor software reset
    }
  }
}

//set clock
void FED3::SetClock(){

  time_t nowTime = now();
  unixtime = nowTime;
  setTime(unixtime);

  /********************************************************
       Display date and time of RTC
     ********************************************************/
  display.setCursor(1, 40);
  display.print ("RTC set to:");
  display.setCursor(1, 40);
  display.print ("RTC set to:");

  display.fillRoundRect (0, 45, 400, 25, 1, WHITE);
  //display.refresh();
  display.setCursor(1, 60);
  if (month(nowTime) < 10)
    display.print('0');      // Trick to add leading zero for formatting
  display.print(month(nowTime), DEC);
  display.print("/");
  if (day(nowTime) < 10)
    display.print('0');      // Trick to add leading zero for formatting
  display.print(day(nowTime), DEC);
  display.print("/");
  display.print(year(nowTime), DEC);
  display.print(" ");
  display.print(hour(nowTime), DEC);
  display.print(":");
  if (minute(nowTime) < 10)
    display.print('0');      // Trick to add leading zero for formatting
  display.print(minute(nowTime), DEC);
  display.print(":");
  if (second(nowTime) < 10)
    display.print('0');      // Trick to add leading zero for formatting
  display.println(second(nowTime), DEC);
  display.drawFastHLine(30, 80, 100, BLACK);
  display.refresh();

  if (digitalRead(LEFT_POKE) == LOW) {
    tone (BUZZER, 800, 1);
    setTime(now() - 60);
    EndTime = millis();
  }

  if (digitalRead(RIGHT_POKE) == LOW) {
    tone (BUZZER, 800, 1);
    setTime(now() + 60);
    EndTime = millis();
  }
  Teensy3Clock.set(now());
}

//Read battery level
void FED3::ReadBatteryLevel() {
  analogReadResolution(12);
  measuredvbat = analogRead(VBATPIN) * 3.3 / 4096.0 * 2.0;
}

/**************************************************************************************************************************************************
                                                                                               Interrupts and sleep
**************************************************************************************************************************************************/
void FED3::disableSleep(){
  EnableSleep = false;                             
}

void FED3::enableSleep(){
  EnableSleep = true;                             
}

//What happens when left poke is poked
void FED3::leftTrigger() {
  if (!leftHeld && digitalRead(LEFT_POKE) == LOW ) {
      Left = true;
      leftHeld = true;
  }
}

//What happens when right poke is poked
void FED3::rightTrigger() {
  if (!rightHeld && digitalRead(RIGHT_POKE) == LOW ) {
    Right = true;
    rightHeld = true;
  }
}

//Sleep function
void FED3::goToSleep() {
  if (EnableSleep==true){
    ReleaseMotor();
    delay (5000); //let things settle  //Wake up every 5 sec to check the pellet well
  }    
}

//Pull all motor pins low to de-energize stepper and save power, also disable motor driver with the EN pin
void FED3::ReleaseMotor () {
  digitalWrite(L_IN1, LOW);
  digitalWrite(L_IN2, LOW);
  digitalWrite(L_IN3, LOW);
  digitalWrite(L_IN4, LOW);
  digitalWrite(R_IN1, LOW);
  digitalWrite(R_IN2, LOW);
  digitalWrite(R_IN3, LOW);
  digitalWrite(R_IN4, LOW);
  if (EnableSleep==true){
    digitalWrite(MOTOR_ENABLE_LEFT, LOW);  //disable motor driver and neopixels
    digitalWrite(MOTOR_ENABLE_RIGHT, LOW);
  }
}

/**************************************************************************************************************************************************
                                                                                               Startup Functions
**************************************************************************************************************************************************/
//Constructor
FED3::FED3(void) {};

//Import Sketch variable from the Arduino script
FED3::FED3(String sketch) {
  sessiontype = sketch;
}

//  dateTime function
void dateTime(uint16_t* date, uint16_t* time) {
  time_t nowTime = now();
  // return date using FAT_DATE macro to format fields
  *date = FAT_DATE(year(nowTime), month(nowTime), day(nowTime));

  // return time using FAT_TIME macro to format fields
  *time = FAT_TIME(hour(nowTime), minute(nowTime), second(nowTime));
}

void FED3::begin() {
  Serial.begin(9600);
  Serial.println(F("→ begin(): pin init"));
  setSyncProvider(Teensy3Clock.get);   // pull time from hardware RTC
  if (timeStatus() != timeSet) {
    setTime(2025, 1, 1, 0, 0, 0);    // emergency fallback – keeps FAT filenames legal
  }
  // Initialize pins

  pinMode(LEFT_POKE, INPUT_PULLUP);
  pinMode(RIGHT_POKE, INPUT_PULLUP);
  pinMode(VBATPIN, INPUT);
  pinMode(MOTOR_ENABLE_LEFT, OUTPUT);
  pinMode(MOTOR_ENABLE_RIGHT, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(L_IN1, OUTPUT);
  pinMode(L_IN2, OUTPUT);
  pinMode(L_IN3, OUTPUT);
  pinMode(L_IN4, OUTPUT);
  pinMode(R_IN1, OUTPUT);
  pinMode(R_IN2, OUTPUT);
  pinMode(R_IN3, OUTPUT);
  pinMode(R_IN4, OUTPUT);
  pinMode(BNC_OUT, OUTPUT);
  pinMode(MPR121_IRQ, INPUT_PULLUP);
  mprWire->setSDA(MPR121_SDA);      // pins 25 / 24
  mprWire->setSCL(MPR121_SCL);
  mprWire->begin();                 // start Wire2

  //initilize the MPR121
// initialise touch sensor on Wire2 with thresholds and autoconfig
  if (!cap.begin(0x5A, mprWire, 9, 4, true)) {   //  ← add the last three arguments
    Serial.println("MPR121 not found. Check wiring.");
    error(6);
  }

  cap.setThresholds(9, 4); // Set touch and release thresholds
  attachInterrupt(digitalPinToInterrupt(MPR121_IRQ), outsideLickIRQ, FALLING); // Attach interrupt for MPR121

  // Initialize RTC
  //rtc.begin();'

  // Initialize Neopixels
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  // Initialize stepper
  digitalWrite(MOTOR_ENABLE_LEFT, LOW);  // Disable left motor driver
  digitalWrite(MOTOR_ENABLE_RIGHT, LOW); // Disable right motor driver

  // Initialize display
  display.begin();
  //const int minorHalfSize = min(display.width(), display.height()) / 2;
  display.setFont(&FreeSans9pt7b);
  display.setRotation(3);
  display.setTextColor(BLACK);
  display.setTextSize(1);
 
  //Is AHT20 temp humidity sensor present?
  if (aht.begin()) {
    tempSensor = true;
  }
 
  // Initialize SD card and create the datafile
  #if !defined(__IMXRT1062__)  // Teensy 4.0/4.1
  SdFile::dateTimeCallback(dateTime);
  #endif
  Serial.println(F("→ begin(): CreateFile()"));
  CreateFile();
  Serial.println(F("returned from CreateFile"));
  CreateDataFile();
  Serial.println(F("returned from CreateDataFile"));
  writeHeader();
  Serial.println(F("returned from writeHeader"));
  // Initialize interrupts
  pointerToFED3 = this;
  attachInterrupt(digitalPinToInterrupt(LEFT_POKE), outsideLeftTriggerHandler, FALLING);
  attachInterrupt(digitalPinToInterrupt(RIGHT_POKE), outsideRightTriggerHandler, FALLING);
  // Create data file for current session
  
  EndTime = 0;
  
  //read battery level
  ReadBatteryLevel();
  
  // Startup display uses StartScreen() unless ClassicFED3==true, then use ClassicMenu()
  if (ClassicFED3 == true){
    ClassicMenu();
  }
  else if (FED3Menu == true){
    FED3MenuScreen();
  }

  else if (psygene) {
    psygeneMenu();
  }

  else {
    StartScreen();
  }
  display.clearDisplay();
  display.refresh();
}

void FED3::FED3MenuScreen() {
  display.clearDisplay();
  display.setCursor(1, 135);
  display.print(filename);
  display.setCursor(10, 20); display.println("FED3 Menu");
  display.setCursor(11, 20); display.println("FED3 Menu");
  display.fillRect(0, 30, 160, 80, WHITE);
  display.setCursor(10, 40);
  display.print("Select Mode:");
  
  display.setCursor(10, 60);
  //Text to display selected FR ratio
  if (FEDmode == 0) display.print("Mode 1");
  if (FEDmode == 1) display.print("Mode 2");
  if (FEDmode == 2) display.print("Mode 3");
  if (FEDmode == 3) display.print("Mode 4");
  if (FEDmode == 4) display.print("Mode 5");
  if (FEDmode == 5) display.print("Mode 6");
  if (FEDmode == 6) display.print("Mode 7");
  if (FEDmode == 7) display.print("Mode 8");
  if (FEDmode == 8) display.print("Mode 9");
  if (FEDmode == 9) display.print("Mode 10");
  if (FEDmode == 10) display.print("Mode 11");
  if (FEDmode == 11) display.print("Mode 12");
  DisplayMouse();
  display.clearDisplay();
  display.refresh();
}

// Set FEDMode
void FED3::SelectMode() {
  // Mode select on startup screen
  //If both pokes are activated
  if ((digitalRead(LEFT_POKE) == LOW) && (digitalRead(RIGHT_POKE) == LOW)) {
    tone (BUZZER, 3000, 500);
    colorWipe(strip.Color(2, 2, 2), 40); // Color wipe
    colorWipe(strip.Color(0, 0, 0), 20); // OFF
    EndTime = millis();
    SetFED = true;
    setTimed = true;
    SetDeviceNumber();
  }

  //If Left Poke is activated
  else if (digitalRead(LEFT_POKE) == LOW) {
    EndTime = millis();
    FEDmode -= 1;
    tone (BUZZER, 2500, 200);
    colorWipe(strip.Color(2, 0, 2), 40); // Color wipe
    colorWipe(strip.Color(0, 0, 0), 20); // OFF
    
    if (psygene) {
      if (FEDmode == -1) FEDmode = 3;
    }
    else {
      if (FEDmode == -1) FEDmode = 11;
    }
    
  }

  //If Right Poke is activated
  else if (digitalRead(RIGHT_POKE) == LOW) {
    EndTime = millis();
    FEDmode += 1;
    tone (BUZZER, 2500, 200);
    colorWipe(strip.Color(2, 2, 0), 40); // Color wipe
    colorWipe(strip.Color(0, 0, 0), 20); // OFF

    if (psygene) {
      if (FEDmode == 4) FEDmode = 0; 
    }

    else {
      if (FEDmode == 12) FEDmode = 0;
    }
  }

  //Double check that modes never go over
  if (psygene) {
    if (FEDmode < 0) FEDmode = 0;
    if (FEDmode > 3) FEDmode = 3;
  }

  else {
    if (FEDmode < 0) FEDmode = 0;
    if (FEDmode > 11) FEDmode = 11;
  }

  display.fillRect (10, 48, 200, 50, WHITE);  //erase the selected program text
  display.setCursor(10, 60);  //Display selected program

  //In classic mode we pre-specify these programs names
  if (ClassicFED3==true){
    if (FEDmode == 0) display.print("Free feeding");
    if (FEDmode == 1) display.print("FR1");
    if (FEDmode == 2) display.print("FR3");
    if (FEDmode == 3) display.print("FR5");
    if (FEDmode == 4) display.print("Progressive Ratio");
    if (FEDmode == 5) display.print("Extinction");
    if (FEDmode == 6) display.print("Light tracking");
    if (FEDmode == 7) display.print("FR1 (Reversed)");
    if (FEDmode == 8) display.print("Prog Ratio (Rev)");
    if (FEDmode == 9) display.print("Self-Stim");
    if (FEDmode == 10) display.print("Self-Stim (Rev)");
    if (FEDmode == 11) display.print("Timed feeding");
    display.refresh();
  }

  else if (psygene) {
    if (FEDmode == 0) display.print("Bandit_100_0");
    if (FEDmode == 1) display.print("FR1");
    if (FEDmode == 2) display.print("Bandit_80_20");
    if (FEDmode == 3) display.print("PR1");
    display.refresh();
  }
  
  //Otherwise we don't know them and just use Mode 1 through Mode 4
  else{
    if (FEDmode == 0) display.print("Mode 1");
    if (FEDmode == 1) display.print("Mode 2");
    if (FEDmode == 2) display.print("Mode 3");
    if (FEDmode == 3) display.print("Mode 4");
    if (FEDmode == 4) display.print("Mode 5");
    if (FEDmode == 5) display.print("Mode 6");
    if (FEDmode == 6) display.print("Mode 7");
    if (FEDmode == 7) display.print("Mode 8");
    if (FEDmode == 8) display.print("Mode 9");
    if (FEDmode == 9) display.print("Mode 10");
    if (FEDmode == 10) display.print("Mode 11");
    if (FEDmode == 11) display.print("Mode 12");
    display.refresh();
  }
  
  while (millis() - EndTime < 1500) {
    SelectMode();
  }
  display.setCursor(10, 100);
  display.println("...Selected!");
  display.refresh();
  delay (500);
  writeFEDmode();
  delay (200);
  teensyReset();     // processor software reset
}

/******************************************************************************************************************************************************
                                                                                           Classic FED3 functions
******************************************************************************************************************************************************/

//  Classic menu display
void FED3::ClassicMenu () {
  //  0 Free feeding
  //  1 FR1
  //  2 FR3
  //  3 FR5
  //  4 Progressive Ratio
  //  5 Extinction
  //  6 Light tracking FR1 task
  //  7 FR1 (reversed)
  //  8 PR (reversed)
  //  9 self-stim
  //  10 self-stim (reversed)
  //  11 time feeding

  // Set FR based on FEDmode
  if (FEDmode == 0) FR = 0;  // free feeding
  if (FEDmode == 1) FR = 1;  // FR1 spatial tracking task
  if (FEDmode == 2) FR = 3;  // FR3
  if (FEDmode == 3) FR = 5; // FR5
  if (FEDmode == 4) FR = 99;  // Progressive Ratio
  if (FEDmode == 5) { // Extinction
    FR = 1;
    ReleaseMotor ();
    digitalWrite (MOTOR_ENABLE, LOW);  //disable motor driver and neopixels
    delay(2); //let things settle
  }
  if (FEDmode == 6) FR = 1;  // Light tracking
  if (FEDmode == 7) FR = 1; // FR1 (reversed)
  if (FEDmode == 8) FR = 1; // PR (reversed)
  if (FEDmode == 9) FR = 1; // self-stim
  if (FEDmode == 10) FR = 1; // self-stim (reversed)

  display.clearDisplay();
  display.setCursor(1, 135);
  display.print(filename);

  display.fillRect(0, 30, 160, 80, WHITE);
  display.setCursor(10, 40);
  display.print("Select Program:");
  
  display.setCursor(10, 60);
  //Text to display selected FR ratio
  if (FEDmode == 0) display.print("Free feeding");
  if (FEDmode == 1) display.print("FR1");
  if (FEDmode == 2) display.print("FR3");
  if (FEDmode == 3) display.print("FR5");
  if (FEDmode == 4) display.print("Progressive Ratio");
  if (FEDmode == 5) display.print("Extinction");
  if (FEDmode == 6) display.print("Light tracking");
  if (FEDmode == 7) display.print("FR1 (Reversed)");
  if (FEDmode == 8) display.print("Prog Ratio (Rev)");
  if (FEDmode == 9) display.print("Self-Stim");
  if (FEDmode == 10) display.print("Self-Stim (Rev)");
  if (FEDmode == 11) display.print("Timed feeding");
  
  DisplayMouse();
  display.clearDisplay();
  display.refresh();
}

//write a FEDmode file (this contains the last used FEDmode)
void FED3::writeFEDmode() {
  ratiofile = SD.open("FEDmode.csv", FILE_WRITE);
  ratiofile.seek(0);
  ratiofile.println(FEDmode);
  ratiofile.flush();
  ratiofile.close();

  startfile = SD.open("start.csv", FILE_WRITE);
  startfile.seek(0);
  startfile.println(timedStart);
  startfile.flush();
  startfile.close();

  stopfile = SD.open("stop.csv", FILE_WRITE);
  stopfile.seek(0);
  stopfile.println(timedEnd);
  stopfile.flush();
  stopfile.close();
}

/******************************************************************************************************************************************************
                                                                                           psygeneMenu
******************************************************************************************************************************************************/

//  Classic menu display
void FED3::psygeneMenu () {
  //  0 Bandit
  //  1 FR1
  //  2 PR1
  //  3 Extinction

  display.clearDisplay();
  display.setCursor(1, 135);
  display.print(filename);

  display.fillRect(0, 30, 160, 80, WHITE);
  display.setCursor(10, 40);
  display.print("Select Program:");
  
  display.setCursor(10, 60);
  //Text to display selected FR ratio
  if (FEDmode == 0) display.print("Bandit_100_0");
  if (FEDmode == 1) display.print("FR1");
  if (FEDmode == 2) display.print("Bandit_80_20");
  if (FEDmode == 3) display.print("PR1");
  
  DisplayMouse();
  display.clearDisplay();
  display.refresh();
}