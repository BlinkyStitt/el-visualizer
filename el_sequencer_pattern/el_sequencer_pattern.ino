/* Control a standalone Sparkfun EL Sequencer in Hannah's Jacket
 *
 * The lights blink randomly
 */

#define NUM_OUTPUTS 3
#define OUTPUT_A 2
#define OUTPUT_B 3
#define OUTPUT_C 4

const int     outputPins[NUM_OUTPUTS] = {OUTPUT_A, OUTPUT_B, OUTPUT_C};
unsigned int  counter = 0;
unsigned int  onMs = 1500;
unsigned int  minOnMs = 1000;
unsigned int  maxOnMs = 2000;

// from http://forum.arduino.cc/index.php?topic=43424.0
// generate a value between 0 <= x < n, thus there are n possible outputs
int rand_range(int n) {
  int r, ul;
  ul = RAND_MAX - RAND_MAX % n;
  while ((r = rand()) >= ul) {
    ;  // get a random number that is fair
  }
  return r % n;
}

void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.println("Starting...");

  for (int outputId = 0; outputId < NUM_OUTPUTS; outputId++) {
    pinMode(outputPins[outputId], OUTPUT);
  }

  // blink all the outputs NUM_OUTPUTS times
  Serial.print("Blinking ");
  Serial.print(NUM_OUTPUTS);
  Serial.println(" times...");
  for (int i = 0; i < NUM_OUTPUTS; i++) {
    for (int outputId = 0; outputId < NUM_OUTPUTS; outputId++) {
      digitalWrite(outputPins[outputId], HIGH);
    }

    delay(200);

    for (int outputId = 0; outputId < NUM_OUTPUTS; outputId++) {
      digitalWrite(outputPins[outputId], LOW);
    }
  }
  delay(500);

  // turn all the outputs off
  for (unsigned int i = 0; i < NUM_OUTPUTS; i++) {
    digitalWrite(outputPins[i], LOW);
  }
  delay(100);

  Serial.println("Starting...");
}

void loop() {
  // todo: pick a random duration for the upcoming pattern
  // todo: do it as a +/- from the previous so it doesn't bounce from super fast to super slow
  // todo: make sure minOnMs <= onMs <= maxOnMs

  switch (rand_range(20)) {  // keep the range at our max case +2 so the default has a chance
    case 0:
      Serial.println(" 0 | 0 | 0 |");
      digitalWrite(outputPins[0], LOW);
      digitalWrite(outputPins[1], LOW);
      digitalWrite(outputPins[2], LOW);
      // todo: decrease onMs
      break;
    case 1:
    case 2:
    case 3:
      Serial.println(" 1 | 0 | 0 |");
      digitalWrite(outputPins[0], HIGH);
      digitalWrite(outputPins[1], LOW);
      digitalWrite(outputPins[2], LOW);
      break;
    case 4:
    case 5:
    case 6:
      Serial.println(" 0 | 1 | 0 |");
      digitalWrite(outputPins[0], LOW);
      digitalWrite(outputPins[1], HIGH);
      digitalWrite(outputPins[2], LOW);
      break;
    case 7:
    case 8:
    case 9:
      Serial.println(" 0 | 0 | 1 |");
      digitalWrite(outputPins[0], LOW);
      digitalWrite(outputPins[1], LOW);
      digitalWrite(outputPins[2], HIGH);
      break;
    case 10:
    case 11:
    case 12:
      Serial.println(" 1 | 1 | 0 |");
      digitalWrite(outputPins[0], HIGH);
      digitalWrite(outputPins[1], HIGH);
      digitalWrite(outputPins[2], LOW);
      break;
    case 13:
    case 14:
    case 15:
      Serial.println(" 1 | 0 | 1 |");
      digitalWrite(outputPins[0], HIGH);
      digitalWrite(outputPins[1], LOW);
      digitalWrite(outputPins[2], HIGH);
      break;
    case 16:
    case 17:
    case 18:
      Serial.println(" 0 | 1 | 1 |");
      digitalWrite(outputPins[0], LOW);
      digitalWrite(outputPins[1], HIGH);
      digitalWrite(outputPins[2], HIGH);
      break;
    default:
      Serial.println(" 1 | 1 | 1 |");
      digitalWrite(outputPins[0], HIGH);
      digitalWrite(outputPins[1], HIGH);
      digitalWrite(outputPins[2], HIGH);
      // todo: increase on Ms
      break;
  }

  delay(onMs);
}
