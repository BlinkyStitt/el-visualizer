#define MAX_WIRES 6
#define INPUT_A A2
#define INPUT_B A3
#define INPUT_C A4
#define INPUT_D A5
#define INPUT_E A6  // analog only
#define INPUT_F A7  // analog only
//#define INPUT_G ??  // 
//#define INPUT_H ??  // 
#define OUTPUT_A 2
#define OUTPUT_B 3
#define OUTPUT_C 4
#define OUTPUT_D 5
#define OUTPUT_E 6
#define OUTPUT_F 7
#define OUTPUT_G 8
#define OUTPUT_H 9
#define MAX_OFF_MS 7000
const int inputPins[MAX_WIRES] = {INPUT_A, INPUT_B, INPUT_C, INPUT_D, INPUT_E, INPUT_F}; //, INPUT_G, INPUT_H};
const int outputPins[MAX_WIRES] = {OUTPUT_A, OUTPUT_B, OUTPUT_C, OUTPUT_D, OUTPUT_E, OUTPUT_F}; //, OUTPUT_G, OUTPUT_H};

#include <elapsedMillis.h>

void setup() {
  Serial.begin(9600);  // TODO! disable this on production build

  for (int i=0; i<MAX_WIRES; i++) {
    pinMode(inputPins[i], INPUT);   // INPUT_PULLUP?
    pinMode(outputPins[i], OUTPUT);
  }

  randomSeed(analogRead(A0));

  Serial.println("Starting in 1 second...");
  delay(1000);
}

elapsedMillis blinkTime = 0;
unsigned int blinkOutput = OUTPUT_A;
unsigned int blinkDuration = random(250, 500);   // keep this less than 1000

void loop() {
  for (int i=0; i<MAX_WIRES; i++){
    if (analogRead(inputPins[i]) > 500) {  // read everything as analog since some pins can't do digital reads
      digitalWrite(outputPins[i], HIGH);
      Serial.print("  X  ");
      // reset blinkTime any time lights turn on
      blinkTime = 0;
    } else {
      // TODO: logic to keep lights on for a minimum amount of time like on the teensy?
      digitalWrite(outputPins[i], LOW);
      Serial.print("     ");
    }
  }
  Serial.println();

  // TODO! this needs a knob for numOutputs otherwise it might blink on unplugged wires
  /*
  if (blinkTime < MAX_OFF_MS) {
    ; // do nothing. leave the lights off for up to MAX_OFF_MS seconds
  } else {
    unsigned long checkTime = blinkTime - MAX_OFF_MS;

    // blink for 1/4 to 3/4 of each second
    if (checkTime < blinkDuration) {
      // turn the EL wire is on
      digitalWrite(outputPins[blinkOutput], HIGH);
    } else if (checkTime < 1000) {
      // turn the EL wire off
      digitalWrite(outputPins[blinkOutput], LOW);
    } else {
      // make sure the EL wire is off (the previous else should have done this, but just in case)
      digitalWrite(outputPins[blinkOutput], LOW);

      // pick a random wire for the next blink
      blinkOutput = random(numOutputs);

      // reset blinkTime to the point where we will keep blinking if no inputs are HIGH
      blinkTime = MAX_OFF_MS;
      // blink the next wire for a random amount of time
      blinkDuration = random(250, 750);
    }
  }
  */
}
