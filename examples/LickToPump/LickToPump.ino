#include <TwoBottle.h>
String sketch = "LickToPump";       
FED3 fed3(sketch);

void setup() {
  fed3.begin();                      // init TwoBottle hardware + SD logging
  fed3.disableSleep();               // poll continuously
}

void loop() {
  fed3.run();                        // updates touch, display, time, etc.

  // Left-side free-deliver
  if (fed3.lickLeftFlag) {
    fed3.FeedLeft();                 // rotate & log a drop
    // clear the flag so we only fire once per lick
    fed3.lickLeftFlag = false;
  }

  // (Optionally) Right-side free-deliver
  if (fed3.lickRightFlag) {
    fed3.FeedRight();
    fed3.lickRightFlag = false;
  }
}
