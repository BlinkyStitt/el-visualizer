#define NUM_OUTPUTS 6  // TODO. a button on the board to configure this would be nice, but we need spare pins
#define MAX_WIRES 6  // todo: multiplexer to get to 8?
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
#define MAX_OFF_MS 12000
const int inputPins[MAX_WIRES] = {INPUT_A, INPUT_B, INPUT_C, INPUT_D, INPUT_E, INPUT_F}; //, INPUT_G, INPUT_H};
const int outputPins[MAX_WIRES] = {OUTPUT_A, OUTPUT_B, OUTPUT_C, OUTPUT_D, OUTPUT_E, OUTPUT_F}; //, OUTPUT_G, OUTPUT_H};

#include <elapsedMillis.h>

void setup() {
  Serial.begin(9600);  // TODO! disable this on production build

  for (int i=0; i<MAX_WIRES; i++) {
    pinMode(inputPins[i], INPUT);
    pinMode(outputPins[i], OUTPUT);
  }

  randomSeed(analogRead(A0));

  Serial.println("Starting in 1 second...");
  delay(1000);
}

elapsedMillis blinkTime = 0;
unsigned int blinkOutput = OUTPUT_A;
unsigned int blinkDuration = random(500, 750);   // keep this less than 1000
int outputState[MAX_WIRES];

void loop() {
  for (int i=0; i<MAX_WIRES; i++) {
    if (analogRead(inputPins[i]) > 500) {  // read everything as analog since some pins can't do digital reads
      outputState[i] = HIGH;
      blinkTime = 0;
    } else {
      // TODO: logic to keep lights on for a minimum amount of time like on the teensy?
      outputState[i] = LOW;
    }
  }

  if (blinkTime < MAX_OFF_MS) {
    // we turned a light on recently. send output states to the wires
    Serial.print(blinkTime);
    for (int i=0; i<MAX_WIRES; i++) {
      digitalWrite(outputPins[i], outputState[i]);
      Serial.print("  ");
      Serial.print(outputState[i]);
      Serial.print("  ");
    }
    Serial.println();
  } else {
    unsigned long checkTime = blinkTime - MAX_OFF_MS;

    // todo: configurable blink modes. waterfall looks cool
    // blink for 1/4 to 3/4 of each second on a random wire
    if (checkTime < blinkDuration) {
      // turn the EL wire is on
      digitalWrite(outputPins[blinkOutput], HIGH);
    } else if (checkTime < 1000) {
      // turn the EL wire off
      digitalWrite(outputPins[blinkOutput], LOW);
    } else {
      // make sure the EL wire is off (the previous else should have done this, but just in case)
      digitalWrite(outputPins[blinkOutput], LOW);

      // pick a random wire for the next blink. don't repeat wires
      int oldBlinkOutput = blinkOutput;
      while (blinkOutput == oldBlinkOutput) {
        blinkOutput = random(NUM_OUTPUTS);
      }
      Serial.print("Next wire: ");
      Serial.println(blinkOutput);

      // reset blinkTime to the point where we will keep blinking if no inputs are HIGH
      blinkTime = MAX_OFF_MS;
      // blink the next wire for a random amount of time
      blinkDuration = random(250, 750);
    }
  }
}
