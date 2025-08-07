#include <TwoBottle.h>  

// — Sketch identifier (will be logged in the CSV) —
String sketch = "PR1_dual";

// — Create the TwoBottle object —
FED3 fed3(sketch);

// — Per‐side counters & current ratio requirements —
int left_poke_count       = 0;
int required_left_pokes   = 1;
int right_poke_count      = 0;
int required_right_pokes  = 1;

void setup() {
  fed3.begin();  
  // initialize the library’s FR field to match our starting ratio
  fed3.FR = required_left_pokes; 
  fed3.disableSleep(); 
}

void loop() {
  fed3.run();  // must be called each loop

  // — LEFT poke handling —
  if (fed3.Left) {
    fed3.logLeftPoke();  
    left_poke_count++;

    if (left_poke_count >= required_left_pokes) {
      // animal has met the current ratio
      fed3.ConditionedStimulus();      // tone + lights
      fed3.FeedLeft();                 // drop a left‐well pellet

      // bump the requirement for the next left‐well pellet
      required_left_pokes++;
      fed3.FR = required_left_pokes;   // update the logged FR field

      // reset for next run
      left_poke_count = 0;
    }
    else {
      fed3.Click();  // feedback click on each sub‐ratio poke
    }

    // clear the poke flag
    fed3.Left = false;
  }

  // — RIGHT poke handling —
  if (fed3.Right) {
    fed3.logRightPoke();
    right_poke_count++;

    if (right_poke_count >= required_right_pokes) {
      fed3.ConditionedStimulus();
      fed3.FeedRight();                // drop a right‐well pellet

      required_right_pokes++;
      fed3.FR = required_right_pokes;  // update FR for logging

      right_poke_count = 0;
    }
    else {
      fed3.Click();
    }

    fed3.Right = false;
  }

  // — Licks are serviced & logged automatically by serviceLicks() inside fed3.run() :contentReference[oaicite:5]{index=5} :contentReference[oaicite:6]{index=6} —
}
