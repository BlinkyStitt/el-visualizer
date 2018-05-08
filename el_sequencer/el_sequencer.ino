#include <elapsedMillis.h>
#include <SoftwareSerial.h>

// TODO: what pins should we use?
#define MASTER_TX A3
#define MASTER_RX A4

// TODO: we don't need RX
SoftwareSerial mySerial(MASTER_RX, MASTER_TX);  // RX, TX

#define NUM_OUTPUTS 8  // 1-8
#define OUTPUT_A 2
#define OUTPUT_B 3
#define OUTPUT_C 4
#define OUTPUT_D 5
#define OUTPUT_E 6
#define OUTPUT_F 7
#define OUTPUT_G 8
#define OUTPUT_H 9
const int outputPins[NUM_OUTPUTS] = {OUTPUT_A, OUTPUT_B, OUTPUT_C, OUTPUT_D, OUTPUT_E, OUTPUT_F, OUTPUT_G, OUTPUT_H};

// data from the Teensy maps to the 8 outputs
unsigned char data = 0;

// the teensy also has this logic. set this value as low as looks good with your lights.
unsigned int minOnMs = 110;
unsigned long turnOffMsArray[NUM_OUTPUTS];

elapsedMillis elapsedMs = 0;    // todo: do we care if this overflows?


void setup() {
  Serial.begin(115200);    // TODO: tune this

  mySerial.begin(115200);    // TODO: tune this

  // pinMode(MASTER_TX, OUTPUT);
  pinMode(MASTER_RX, INPUT);

  for (int i = 0; i < NUM_OUTPUTS; i++) {
    Serial.print(i);
    pinMode(outputPins[i], OUTPUT);
  }

  Serial.println("Started...");
}


void loop() {
  if (mySerial.available()) {
    data = mySerial.read();

    for (int i = 0; i < NUM_OUTPUTS; i++) {
      if (bitRead(data, i) == 1) {
        // TODO: make sure that we don't turn on more than 6 lights
        // TODO: I think we need to loop once to check if the lights should turn off and then loop again so that numOn is correct
        digitalWrite(outputPins[i], HIGH);

        // make sure we stay on for a minimum amount of time
        turnOffMsArray[i] = elapsedMs + minOnMs;
        Serial.print("| 1 ");
      } else {
        if (elapsedMs < turnOffMsArray[i]) {
          // the output has not been on for long enough. leave it on.
          Serial.print("| 0 ");
        } else {
          // the output has been on for at least minOnMs and is quiet now. turn it off
          digitalWrite(outputPins[i], LOW);
          Serial.print("|   ");
        }
      }
    }
    Serial.println();
  }

  // while we wait for new data from the Teensy
  // check to see if we should turn anything off
  for (int i = 0; i < NUM_OUTPUTS; i++) {
    if (bitRead(data, i) == 0) {
      // this output should be off
      if (elapsedMs < turnOffMsArray[i]) {
        // the output has not been on for long enough to prevent flickering. leave it on.
      } else {
        // the output has been on for at least minOnMs and is quiet now. turn it off
        // we do this every iteration to be as responsive as possible
        digitalWrite(outputPins[i], LOW);

        // flip the bit so we don't bother checking this output again
        bitSet(data, i);
      }
    }
  }
}
