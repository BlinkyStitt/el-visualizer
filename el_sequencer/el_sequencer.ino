#include <SoftwareSerial.h>

#define MASTER_TX A4
#define MASTER_RX A3

// TODO: we don't need RX
SoftwareSerial mySerial(MASTER_RX, MASTER_TX);  // RX, TX

#define NUM_OUTPUTS 8
#define OUTPUT_A 2
#define OUTPUT_B 3
#define OUTPUT_C 4
#define OUTPUT_D 5
#define OUTPUT_E 6
#define OUTPUT_F 7
#define OUTPUT_G 8
#define OUTPUT_H 9
const int outputPins[NUM_OUTPUTS] = {OUTPUT_A, OUTPUT_B, OUTPUT_C, OUTPUT_D, OUTPUT_E, OUTPUT_F, OUTPUT_G, OUTPUT_H};


void setup() {
  Serial.begin(115200);    // TODO: tune this

  delay(200);
  Serial.println("Starting...");

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
    unsigned char data = mySerial.read();

    for (int i = 0; i < NUM_OUTPUTS; i++) {
      if (bitRead(data, i) == 1) {
        digitalWrite(outputPins[i], HIGH);
        Serial.print(" 1 |");
      } else {
        digitalWrite(outputPins[i], LOW);
        Serial.print("   |");
      }
    }
    Serial.print(" ");

    Serial.println(data);
    Serial.flush();
  }
}
