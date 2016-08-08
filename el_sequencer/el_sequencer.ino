/* Control for the Sparkfun EL Sequencer in Bryan's Sound-reactive EL Jacket
 *
 * It would probably be simpler to use the EL shield, but that doesn't do the
 * 12V -> 3.3V for us and would be more work to solder.
 *
 * When inputs are HIGH, the matching outputs are set to HIGH.
 *
 */

/*
 * END you can easily customize these
 */

#define MAX_WIRES 6  // todo: multiplexer to get to this to 8
#define INPUT_A A2
#define INPUT_B A3
#define INPUT_C A4
#define INPUT_D A5
#define INPUT_E A6  // analog only
#define INPUT_F A7  // analog only
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

void setup() {
  for (int i = 0; i < MAX_WIRES; i++) {
    pinMode(inputPins[i], INPUT);
    pinMode(outputPins[i], OUTPUT);
  }
}

void loop() {
  for (int i = 0; i < MAX_WIRES; i++) {
    // read everything as analog since A6 and A7 can't do digital reads
    int inputValue = analogRead(inputPins[i]);
    if (inputValue > 500) {
      digitalWrite(outputPins[i], HIGH);
    } else {
      digitalWrite(outputPins[i], LOW);
    }
  }
}
