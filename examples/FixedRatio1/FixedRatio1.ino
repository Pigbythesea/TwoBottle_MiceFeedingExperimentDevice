/*

*/

#include <TwoBottle.h>      
String sketch = "FR1";    
FED3 fed3 (sketch);                        

void setup() {
  fed3.begin();
  fed3.disableSleep();
}

void loop() {
  fed3.run();
  if (fed3.Left) { //left poke triggered
    
    // 1) CS
    fed3.ConditionedStimulus();

    // 2) move pellet
    fed3.FeedLeft();

    // 3) log the poke
    fed3.logLeftPoke();
  }
  if (fed3.Right) {
    
    // 1) CS
    fed3.ConditionedStimulus();

    // 2) feed deliver
    fed3.FeedRight();

    // 3) log the poke
    fed3.logRightPoke();

  }
}
