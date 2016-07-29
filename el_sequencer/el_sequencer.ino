/* Control for the Sparkfun EL Sequencer in Bryan's Sound-reactive EL Jacket
 *
 * When inputs are HIGH, outputs are set to HIGH.
 * When there haven't been any inputs for MAX_OFF_MS, blink in pretty ways
 *
 * TODO:
 *  - multiplexer to get to MAX_WIRES to 8
 *  - randomize input -> outputPins every X minutes
 */

/*
 * you can easily customize these
 *
 * todo: define or const?
 */
#define MAX_OFF_MS 7000  // keep this longer than the MAX_OFF_MS in the teensy code
int numOutputs = 1;  // this grows when inputs are turned on  // todo: a button on the board to configure this would be nice, but we need spare pins
/*
 * END you can easily customize these
 */

#define MAX_WIRES 6
#define INPUT_A A2
#define INPUT_B A3
#define INPUT_C A4
#define INPUT_D A5
#define INPUT_E A6  // analog only. we should use this for tuning the number of inputs/outputs
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
const int inputPins[MAX_WIRES] = {INPUT_A, INPUT_B, INPUT_C, INPUT_D, INPUT_E, INPUT_F}; //, INPUT_G, INPUT_H};
const int outputPins[MAX_WIRES] = {OUTPUT_A, OUTPUT_B, OUTPUT_C, OUTPUT_D, OUTPUT_E, OUTPUT_F}; //, OUTPUT_G, OUTPUT_H};

#include <elapsedMillis.h>

/*
 * Helper functions
 */

// from http://forum.arduino.cc/index.php?topic=43424.0
// generate a value between 0 <= x < n, thus there are n possible outputs
int rand_range(int n) {
  int r, ul;

  ul = RAND_MAX - RAND_MAX % n;
  while ((r = rand()) >= ul)
    ;

  return r % n;
}

// from http://forum.arduino.cc/index.php?topic=43424.0
void bubbleUnsort(int *list, int elem) {
  for (int a = elem-1; a > 0; a--) {
    // todo: not sure what is better. apparently the built in random has some sort of bias?
    //int r = random(a+1);
    int r = rand_range(a+1);
    if (r != a) {
      int temp = list[a];
      list[a] = list[r];
      list[r] = temp;
      /*
      // https://betterexplained.com/articles/swap-two-variables-using-xor/
      list[a] = list[a] xor list[r];
      list[r] = list[a] xor list[r];
      list[a] = list[a] xor list[r];
      */
   }
  }
}

/*
 * END Helper functions
 */

int randomizedOutputIds[MAX_WIRES];

void updateNumOutputs() {
  int oldNumOutputs = numOutputs

  // todo: read some input once we figure out how to get another analog input on the board

  if (oldNumOutputs == numOutputs) {
    // no need to do antyhing. numOutputs didn't change
    return
  }

  // put the randomized inputs back in order in case we lowered numOutputs
  for (int i = 0; i<numOutputs; i++) {
    randomizedOutputIds[i] = i;
  }

  // randomize them again
  bubbleUnsort(randomizedOutputIds, numOutputs);
}

void setup() {
  Serial.begin(9600);  // TODO! disable this on production build

  for (int i = 0; i < MAX_WIRES; i++) {
    pinMode(inputPins[i], INPUT);
    pinMode(outputPins[i], OUTPUT);
    randomizedOutputIds[i] = i;
  }

  randomSeed(analogRead(A0));

  updateNumOutputs();

  bubbleUnsort(randomizedOutputIds, numOutputs);

  Serial.println("Starting...");
}

elapsedMillis blinkTime = 0;
bool outputState[MAX_WIRES];

void loop() {
  updateNumOutputs();

  // todo: change this to support 8 inputs once we use a multiplexer
  for (int i = 0; i < MAX_WIRES; i++) {
    int outputId = randomizedOutputIds[i];
    if (analogRead(inputPins[i]) > 500) {  // read everything as analog since some pins can't do digital reads
      outputState[outputId] = HIGH;
      blinkTime = 0;

      if (i > numOutputs) {
        // grow numOutputs to match inputs
        numOutputs = i + 1;
      }
    } else {
      outputState[outputId] = LOW;
    }
  }

  if (blinkTime < MAX_OFF_MS) {
    // we turned a light on recently. send output states to the wires
    Serial.print(blinkTime);
    Serial.print("ms : ");
    for (int i = 0; i < numOutputs; i++) {
      digitalWrite(outputPins[i], outputState[i]);
      if (outputState[i]) {
        Serial.print(outputState[i]);
      } else {
        Serial.print(" ");
      }
      Serial.print(" | ");
    }
    Serial.println();
  } else {
    Serial.println("Blinking randomly...");

    // turn the outputs on in a random order
    // TODO: do this with some functions and without delay calls
    bubbleUnsort(randomizedOutputIds, numOutputs);
    for (int i = 0; i < numOutputs; i++) {
      int outputId = randomizedOutputIds[i];
      Serial.print("Turning on #");
      Serial.println(outputId);
      digitalWrite(outputPins[outputId], HIGH);
      delay(300);
      digitalWrite(outputPins[outputId], LOW);
      delay(150);
      digitalWrite(outputPins[outputId], HIGH);
      delay(300);
      digitalWrite(outputPins[outputId], LOW);
      delay(150);
      digitalWrite(outputPins[outputId], HIGH);
      delay(random(200, 1000));
    }

    // randomize the outputs for when we turn them off
    bubbleUnsort(randomizedOutputIds, numOutputs);

    // wait a random amount of time with the outputs on
    delay(random(2000, 3000));

    // turn all the outputs off slowly in a random order
    for (int i=0; i < numOutputs; i++) {
      int outputId = randomizedOutputIds[i];
      Serial.print("Turning off #");
      Serial.println(outputId);
      digitalWrite(outputPins[outputId], LOW);
      delay(150);
      digitalWrite(outputPins[outputId], HIGH);
      delay(300);
      digitalWrite(outputPins[outputId], LOW);
      delay(150);
      digitalWrite(outputPins[outputId], HIGH);
      delay(300);
      digitalWrite(outputPins[outputId], LOW);
      delay(random(1000, 2000));
    }

    // wait with all the the lights off
    delay(random(500, 750));

    // turn the last output back on
    int lonelyOutputId = randomizedOutputIds[numOutputs - 1];
    Serial.print("Turning on lonely light #");
    Serial.println(lonelyOutputId);
    digitalWrite(outputPins[lonelyOutputId], HIGH);
    delay(300);
    digitalWrite(outputPins[lonelyOutputId], LOW);
    delay(150);
    digitalWrite(outputPins[lonelyOutputId], HIGH);
    delay(300);
    digitalWrite(outputPins[lonelyOutputId], LOW);
    delay(150);
    digitalWrite(outputPins[lonelyOutputId], HIGH);

    // wait a random amount of time with just one light on
    delay(random(10000, 20000));

    // blink the lonely output a couple times
    Serial.print("Blinking lonely light #");
    Serial.println(lonelyOutputId);
    digitalWrite(outputPins[lonelyOutputId], LOW);
    delay(150);
    digitalWrite(outputPins[lonelyOutputId], HIGH);
    delay(300);
    digitalWrite(outputPins[lonelyOutputId], LOW);
    delay(150);
    digitalWrite(outputPins[lonelyOutputId], HIGH);
    delay(300);
    digitalWrite(outputPins[lonelyOutputId], LOW);
    delay(150);
    digitalWrite(outputPins[lonelyOutputId], HIGH);
    delay(300);
    digitalWrite(outputPins[lonelyOutputId], LOW);
    delay(random(2500, 3000));

    Serial.println("Done blinking.");
  }
}
