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

*/

#define VER "1.17.0"

#ifndef TWOBOTTLE_H
#define TWOBOTTLE_H

//include these libraries
#include <Arduino.h>
#include <Adafruit_BusIO_Register.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_I2CRegister.h>
#include <Adafruit_SPIDevice.h>
#include <Wire.h>
extern TwoWire* mprWire;
#include <SPI.h>
#include <TimeLib.h>
#define SD_FAT_TYPE 3
#include <SdFat.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SharpMem.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/Org_01.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_MPR121.h>
#include <Stepper.h>
typedef void (*voidFuncPtr)(void);


// Lightweight Teensy replacement for ArduinoLowPower


// Pin definitions
#define NEOPIXEL        18
#define MOTOR_ENABLE_LEFT    15
#define MOTOR_ENABLE MOTOR_ENABLE_LEFT // alias for functions still calling motor_enable
#define MOTOR_ENABLE_RIGHT   35
#define GREEN_LED       30
#define LEFT_POKE       22
#define RIGHT_POKE      21
#define BUZZER          3
#define VBATPIN         A6
#define BNC_OUT         23
#define SHARP_SCK       12
#define SHARP_MOSI      11
#define SHARP_SS        10
#define MPR121_SDA     25
#define MPR121_SCL     24
#define MPR121_IRQ     9
#define LEFT_LICK 0
#define RIGHT_LICK 1

#define L_IN1 16
#define L_IN2 17
#define L_IN3 14
#define L_IN4 13
#define R_IN1 36
#define R_IN2 37    
#define R_IN3 34
#define R_IN4 33

#define MPR_IRQ

#define BLACK 0
#define WHITE 1
static constexpr int STEPS = 200; // number of steps per revolution for the stepper motors


extern bool Left;

class FED3 {
    // Members
    public:
        FED3(void);
        FED3(String sketch);
        String sketch = "undef";
        String sessiontype = "undef";

        void classInterruptHandler(void);
        void begin();
        void run();
        
        // SD logging
        SdFat SD;
        FsFile logfile;       // Create file object
        FsFile ratiofile;     // Create another file object
        FsFile configfile;    // Create another file object
        FsFile startfile;     // Create another file object
        FsFile stopfile;      // Create another file object
        char filename[22];  // Array for file name data logged to named in setup
        void logdata();
        void CreateFile();
        void CreateDataFile ();
        void writeHeader();
        void writeConfigFile();
        void writeFEDmode();
        void error(uint8_t errno);
        void getFilename(char *filename);
        bool suppressSDerrors = false;  //set to true to suppress SD card errors at startup 

        // Battery
        float measuredvbat = 1.0;
        void ReadBatteryLevel();

        // Neopixel
        void pixelsOn(int R, int G, int B, int W);
        void pixelsOff();
        void Blink(byte PIN, byte DELAY_MS, byte loops);
        void colorWipe(uint32_t c, uint8_t wait);
        void leftPixel(int R, int G, int B, int W);
        void rightPixel(int R, int G, int B, int W);
        void leftPokePixel(int R, int G, int B, int W);
        void rightPokePixel(int R, int G, int B, int W);
        
        // Display functions
        void UpdateDisplay();
        void DisplaySDError();
        void DisplayJamClear();
        void DisplayJammed();
        void DisplayRetrievalInt();
        void DisplayLeftInt();
        void DisplayRightInt();
        void DisplayBattery();
        void DisplayDateTime();
        void DisplayIndicators();
        void DisplayTimedFeeding();
        void DisplayNoProgram();
        void DisplayMinPoke();
        void DisplayMouse();

        // Startup menu function
        void ClassicMenu();
        void StartScreen();
        void FED3MenuScreen();
        void psygeneMenu();
        void SetClock();
        
        //BNC input/output
		void ReadBNC(bool blinkGreen);
        bool BNCinput = false;
        
        // Motor
        void ReleaseMotor();
        int numMotorTurnsLeft = 0;
        int numMotorTurnsRight = 0;
        int doseLeftSteps = 1000;
        int doseRightSteps = 1000;
        int dispenseRPM = 180;

        // Set FED
        void SelectMode();
        void SetDeviceNumber();

        // Stimuli
        void ConditionedStimulus(int duration = 200);
        void Click();
        void Noise(int duration = 200);
        void BNC(int DELAY_MS, int loops);
        void pulseGenerator(int pulse_width, int frequency, int repetitions);

        void Tone(int freq, int duration);
        void stopTone();
        
        // Pelet and poke functions
        void CheckRatio();
        void logLeftPoke();
        void logRightPoke();
        void serviceLicks();
        void logLeftLick();
        void logRightLick();
        void FeedLeft(int steps = 0, int pulse = 0, bool pixelsoff = true);
        void FeedRight(int steps = 0, int pulse = 0, bool pixelsoff = true);
        bool dispenseTimer_ms(int ms);
        void pelletTrigger();
        void leftTrigger();
        void rightTrigger();
        void goToSleep();

        void Timeout(int timeout, bool reset = false, bool whitenoise = false);


        int minPokeTime = 0;
        void randomizeActivePoke(int max);
        int consecutive = 0;
        
        //jam movements
		bool RotateDiskLeft(int steps);
        bool RotateDiskRight(int steps);
        //bool ClearJam();
        //bool VibrateJam();
        //bool MinorJam();

        //timed feeding variables
        int timedStart; //hour to start the timed Feeding session, out of 24 hour clock
        int timedEnd; //hour to start the timed Feeding session, out of 24 hour clock

        // mode variables
        int FED;
        int FR = 1;
        bool DisplayPokes = true;
        bool DisplayTimed = false;
        byte FEDmode = 1;
        byte previousFEDmode = FEDmode;
  
        // event counters
        int LeftCount = 0;
        int RightCount = 0;
        int TotalDeliverCount = 0;
        int LeftDeliverCount = 0;
        int RightDeliverCount = 0;
        int BlockPelletCount = 0;
        int timeout = 0;

        bool countAllPokes = true;
        
        // state variables
        bool activePoke = 1;  // 0 for right, 1 for left, defaults to left poke active
        volatile bool Left = false;
        volatile bool Right = false;
        bool LeftDropAvailable = false; //left pellet well has a drop
        bool RightDropAvailable = false; //right pellet well has a drop
        unsigned long currentHour;
        unsigned long currentMinute;
        unsigned long currentSecond;
        unsigned long displayupdate;
        String Event = "None";   //What kind of event just happened?

        // task variables
        int prob_left = 0;
        int prob_right = 0;
        int pelletsToSwitch = 0;
        bool allowBlockRepeat = false;

        // timing variables
        int retInterval = 0;
        int leftInterval = 0;
        int rightInterval = 0;
        int leftPokeTime = 0.0;
        int rightPokeTime = 0.0;
        unsigned long LeftDropTime = 0;
        unsigned long RightDropTime = 0;
        unsigned long lastPellet = 0;
        unsigned long unixtime = 0;
        int interPelletInterval = 0;

        // flags
        bool Ratio_Met = false;
        bool EnableSleep = true;
        void disableSleep();
        void enableSleep();
        bool ClassicFED3 = false;
        bool FED3Menu = false;
        bool psygene = false;
        bool tempSensor = false;
        volatile bool lickLeftFlag;
        volatile bool lickRightFlag;
        volatile bool leftHeld = false;
        volatile bool rightHeld = false;

        int EndTime = 0;
        int ratio = 1;
        int previousFR = FR;
        int previousFED = FED;

        bool SetFED = false;
        bool setTimed = false;
        
        // Neopixel strip
        Adafruit_NeoPixel strip = Adafruit_NeoPixel(10, NEOPIXEL, NEO_GRBW + NEO_KHZ800);
        // Display
        Adafruit_SharpMem display = Adafruit_SharpMem(SHARP_SCK, SHARP_MOSI, SHARP_SS, 144, 168);
        // Stepper
        Stepper stepperLeft{STEPS, L_IN1, L_IN2, L_IN3, L_IN4};
        Stepper stepperRight{STEPS, R_IN1, R_IN2, R_IN3, R_IN4};
        // Temp/Humidity Sensor
        Adafruit_AHTX0 aht;
        // MPR121 Touch Sensor
        Adafruit_MPR121 cap;
        volatile bool lickIRQ = false;
        uint32_t LeftLickCount = 0;
        uint32_t RightLickCount = 0;

    private:
        static FED3* staticFED;
        static void updatePelletTriggerISR();
        static void updateLeftTriggerISR();
        static void updateRightTriggerISR();
};

#endif
