/*

*/

////////////////////////////////////////////////////
// Customize the FR number and which poke is active
////////////////////////////////////////////////////
int FR = 6;
bool LeftActive = false;                               //Set to false to make right poke active

#include <TwoBottle.h>                                     //Include the TwoBottle library
String sketch = "FRCustom";                           //Unique identifier text for each sketch
FED3 fed3 (sketch);                                   //Start the TwoBottle object

void setup() {
  fed3.begin();                                       //Setup the TwoBottle hardware
  fed3.FR = FR;                                       //Share the FR ratio with the fed3 library so it is logged on the SD card and displayed on the screen
  fed3.disableSleep();
  if (LeftActive == false) {
    fed3.activePoke = 0;                              //update the activepoke variable in the TwoBottle library for logging and display. This defaults to 1, so only set to 0 if LeftActive == false
  }
}
void loop() {
  fed3.run();                                         //Call fed.run at least once per loop

  // If Left poke is triggered
  if (fed3.Left) {
    fed3.Click();                                 //click stimulus
    fed3.logLeftPoke();                               //Log left poke
    if (LeftActive == false) {
      if (fed3.LeftCount % FR == 0) {                 //if fixed ratio is  met
        fed3.ConditionedStimulus();                   //deliver conditioned stimulus (tone and lights)
        fed3.FeedLeft();                                  //deliver pellet
      }
    }
  }

  // If Right poke is triggered
  if (fed3.Right) {
    fed3.logRightPoke();                              //Log Right poke
    fed3.Click();                                 //click stimulus
    if (LeftActive == true) {
      if (fed3.RightCount % FR == 0) {                 //if fixed ratio is  met
        fed3.ConditionedStimulus();                   //deliver conditioned stimulus (tone and lights)
        fed3.FeedRight();                                  //deliver pellet
      }
    }
  }
}
