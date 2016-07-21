#define MAX_WIRES 8
#define INPUT_A 10  // TODO! real input pin
#define INPUT_B 11  // TODO! real input pin
#define INPUT_C 12  // TODO! real input pin
#define INPUT_D 13  // TODO! real input pin
#define INPUT_E 14  // TODO! real input pin
#define INPUT_F 15  // TODO! real input pin
#define INPUT_G 16  // TODO! real input pin
#define INPUT_H 17  // TODO! real input pin
#define OUTPUT_A 2
#define OUTPUT_B 3
#define OUTPUT_C 4
#define OUTPUT_D 5
#define OUTPUT_E 6
#define OUTPUT_F 7
#define OUTPUT_G 8
#define OUTPUT_H 9
#define MAX_OFF_MS 7000
const int inputPins[MAX_WIRES] = {INPUT_A, INPUT_B, INPUT_C, INPUT_D, INPUT_E, INPUT_F, INPUT_G, INPUT_H};
const int outputPins[MAX_WIRES] = {OUTPUT_A, OUTPUT_B, OUTPUT_C, OUTPUT_D, OUTPUT_E, OUTPUT_F, OUTPUT_G, OUTPUT_H};

#include <AnalogPin.h>

AnalogPin                numOutputKnob(A1);  // TODO! real input pin

void setup() {
  for (int i=0; i<MAX_WIRES; i++) {
    pinMode(inputPins[i], INPUT);
    pinMode(outputPins[i], OUTPUT);
  }

  randomSeed(analogRead(0));
}

elapsedMillis blinkTime = 0;
unsigned int blinkOutput = OUTPUT_A;
unsigned int blinkDuration = random(250, 500);   // keep this less than 1000

void loop() {
  // configure number of outputs
  int numOutputs = (int)numOutputKnob.read(1) / (1024 / MAX_WIRES) + 1; // A1 (knob on audio board. pin 15) 0-1023. todo: tune this divisor
  if (numOutputs < 1) {
    numOutputs = 1;
  } else if (numOutputs > MAX_WIRES) {
    numOutputs = MAX_WIRES;
  }
  // todo: have a button to lock this? or maybe a button for more or less. instead of a pot that can bounce around a bit

  for (int i=0; i<MAX_WIRES; i++){
    if (digitalRead(inputPins[i])) {
      digitalWrite(outputPins[i], HIGH);

      // reset blinkTime any time lights turn on
      blinkTime = 0;
    } else {
      digitalWrite(outputPins[i], LOW);
    }
  }

  if (blinkTime < MAX_OFF_MS) {
    ; // do nothing. leave the lights off for up to MAX_OFF_MS seconds
  } else {
    unsigned long checkTime = blinkTime - MAX_OFF_MS;

    // blink for 1/4 to 1/2 of each second
    // TODO! make this random?
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
      blinkOutput = random(MAX_WIRES);

      // reset blinkTime to the point where we will keep blinking if no inputs are HIGH
      blinkTime = MAX_OFF_MS;
      // blink the next wire for a random amount of time
      blinkDuration = random(250, 500);
    }
  }
}
