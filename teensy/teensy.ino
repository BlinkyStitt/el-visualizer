// control for Bryan's Jacket with a configurable number of EL wires
//
// when it is quiet, lights flash out morse code
// when it is loud, lights flash with the music
//

/*
 * you can customize these
 *
 * todo: define or const?
 */
#define BUTTON_HOLD_MS 200
#define BUTTON_INTERVAL 20
#define INPUT_NUM_OUTPUTS_KNOB 15
#define EMA_ALPHA 0.2
#define FFT_AVERAGE_TOGETHER 8  // higher values = slow refresh rate. default 8 = ~344/8 refreshes per second
#define FFT_IGNORED_BINS 1  // skip the first X bins. They are really noisy
#define MAX_OFF_MS 10000
#define MAX_MORSE_ARRAY_LENGTH 256  // todo: tune this
#define MINIMUM_INPUT_SENSITIVITY 0.050  // todo: make this a button that sets this to whatever the current volume is?
#define MINIMUM_INPUT_RANGE 0.50  // activate outputs on sounds that are at least this % as loud as the loudest sound
#define DEFAULT_MORSE_STRING "HELLO WORLD"  // what to blink if no morse.txt on the SD card

const int minimumOnMs = 250;
const int numFftBinsWeCareAbout = 25;  // only the bottom 10-20% of the FFT is worth visualizing

// inspired by http://www.kent-engineers.com/codespeed.htm
const int morseDitMs = 250;
const int morseDahMs = morseDitMs * 3;
const int morseElementSpaceMs = 825;  // 10% longer than a dah
const int morseWordSpaceMs = morseElementSpaceMs * 3;
/*
 * END you can customize these
 */

#define FFT_MAX_BINS 127
#define MAX_OUTPUTS 8
#define OUTPUT_A 0
#define OUTPUT_B 1
#define OUTPUT_C 2
#define OUTPUT_D 3
#define OUTPUT_E 4
#define OUTPUT_F 5
#define OUTPUT_G 8
#define OUTPUT_H 20
#define OUTPUT_ON 0
#define OUTPUT_OFF 1
#define OUTPUT_OFF_NEXT_OUTPUT 2

const int outputPins[MAX_OUTPUTS] = {OUTPUT_A, OUTPUT_B, OUTPUT_C, OUTPUT_D, OUTPUT_E, OUTPUT_F, OUTPUT_G, OUTPUT_H};

#include <Audio.h>
#include <elapsedMillis.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

AudioInputI2S            audioInput;
AudioOutputI2S           audioOutput; // even though we don't output to this, it still needs to be defined for the shield to work
AudioAnalyzeFFT256       myFFT;
AudioConnection          patchCord1(audioInput, audioOutput);
AudioConnection          patchCord2(audioInput, myFFT);
AudioControlSGTL5000     audioShield;

struct MorseCommand {
  unsigned int checkMs;
  unsigned int outputAction;
};
int morse_array_length = MAX_MORSE_ARRAY_LENGTH;
MorseCommand morse_array[MAX_MORSE_ARRAY_LENGTH];


/*
   HELPER FUNCTIONS
*/

void printNumber(float n, float minimum = 0.008) {
  if (n >= minimum) {
    Serial.print(n, 3);
    Serial.print(" ");
  } else {
    Serial.print("   -  "); // don't print "0.00"
  }
}

int morseDit(MorseCommand morse_array[], int i) {
  Serial.print('.');
  // todo: protect overflowing morse_array
  morse_array[i].checkMs = morseDitMs;
  morse_array[i].outputAction = OUTPUT_ON;
  i++;
  return morse_element_space(morse_array, i);
}

int morseDah(MorseCommand morse_array[], int i) {
  Serial.print('-');
  // todo: protect overflowing morse_array
  morse_array[i].checkMs = morseDahMs;
  morse_array[i].outputAction = OUTPUT_ON;
  i++;
  return morse_element_space(morse_array, i);
}

int morse_element_space(MorseCommand morse_array[], int i) {
  // todo: protect overflowing morse_array
  morse_array[i].checkMs = morseElementSpaceMs;
  morse_array[i].outputAction = OUTPUT_OFF;
  i++;
  return i;
}

int morse_word_space(MorseCommand morse_array[], int i) {
  Serial.print(" / ");
  // todo: protect overflowing morse_array
  morse_array[i].checkMs = morseWordSpaceMs;
  morse_array[i].outputAction = OUTPUT_OFF_NEXT_OUTPUT;
  i++;
  return i;
}

/*
MorseCommand[] string2morse(String str) {
  // todo: move all the morse stuff in setup up to here
}
*/

/*
   END HELPER FUNCTIONS
*/

void setup() {
  Serial.begin(9600);  // TODO! disable this on production build

  AudioMemory(10); // todo: tune this. so far max i've seen is 5
  audioShield.enable();
  audioShield.muteHeadphone(); // to avoid any clicks
  audioShield.inputSelect(AUDIO_INPUT_MIC);
  // audioShield.lineInLevel(4);  // 0-15. 5 is default. 0 is highest voltage
  audioShield.volume(0.5);  // for debugging
  audioShield.micGain(63);  // 0-63

  audioShield.audioPreProcessorEnable();  // todo: i dont think post goes to the

  //audioShield.enhanceBassEnable();
  //audioShield.enhanceBass(1.0, -1.0, false, 5);  // todo: tune this

  // bass, mid_bass, midrange, mid_treble, treble
  audioShield.eqSelect(GRAPHIC_EQUALIZER);
  audioShield.eqBands(-1.0, -0.10, 0.10, 0.10, 0.33);  // todo: tune this

  // todo: autoVolume?

  myFFT.averageTogether(FFT_AVERAGE_TOGETHER);

  for (int i=0; i<MAX_OUTPUTS; i++) {
    pinMode(outputPins[i], OUTPUT);
  }

  // setup numOutputs knob
  pinMode(INPUT_NUM_OUTPUTS_KNOB, INPUT);

  // read text off the SD card and translate it into morse code
  String morseString = "";
  SPI.setMOSI(7);
  SPI.setSCK(14);
  if (SD.begin(10)) {
    // todo: support a directory with multiple text files?
    File morseFile = SD.open("morse.txt", FILE_READ);
    if (morseFile) {
      Serial.println("Reading morse.txt...");
      // if the file is available, read it:
      while (morseFile.available()) {
        // todo: this is wrong, but im just testing
        morseString += (char)morseFile.read();
      }
      morseFile.close();
    }
  }

  if (morseString == "") {
    // if the file isn't open, pop up an error:
    Serial.println("No morse.txt...");
    morseString = DEFAULT_MORSE_STRING;
  }

  Serial.print("morseString: ");
  Serial.println(morseString);

  int morse_array_length = 0;
  for (unsigned int i=0; i<morseString.length(); i++) {
    // todo: make sure morse_array_length never exceeds max
    // https://upload.wikimedia.org/wikipedia/en/5/5a/Morse_comparison.svg
    // todo: make everything upcase?
    switch (morseString[i]) {
      case 'E':
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'T':
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'I':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'A':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'N':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'M':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'S':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'U':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'R':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'W':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'D':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'K':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'G':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'O':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'H':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'V':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'F':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case ' ':
        morse_array_length = morse_word_space(morse_array, morse_array_length);
        break;
      case 'L':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'P':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'J':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'B':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'X':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'C':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'Y':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case 'Z':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case 'Q':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case '5':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case '4':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case '3':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case '2':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case '1':
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case '6':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case '7':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case '8':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case '9':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        break;
      case '0':
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        break;
      case '.':
        // todo: do this on a different wire?
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
        morse_array_length = morseDit(morse_array, morse_array_length);
        morse_array_length = morseDah(morse_array, morse_array_length);
      default:
        break;
    }

    Serial.print(' ');
    morse_array_length = morse_element_space(morse_array, morse_array_length);
  }
  Serial.println();

  Serial.print("morse array length: ");
  Serial.println(morse_array_length);

  audioShield.unmuteHeadphone();  // for debugging

  // todo: is this enough?
  randomSeed(analogRead(0));

  Serial.println("Starting in 1 second...");
  delay(1000);
}


elapsedMillis nowMs = 0;

int blinkOutput = OUTPUT_A;
int numOutputs = 1;
int morseOutputId = random(numOutputs);

// Exponential Moving Average of each wire individually and then all the wires together
float avgInputLevel[MAX_OUTPUTS + 1];
float lastLoudestLevel = 0;

unsigned long lastOnMs;
unsigned long lastOnMsArray[MAX_OUTPUTS];
int outputStates[MAX_OUTPUTS];

int lastMorseId = 0;


void loop() {
  // configure number of outputs
  int oldNumOutputs = numOutputs;
  numOutputs = (int)analogRead(INPUT_NUM_OUTPUTS_KNOB) / (1024 / MAX_OUTPUTS) + 1;
  if (oldNumOutputs - numOutputs > 0) {
    // we turned numOutputs down, make sure we turn the outputs off to match
    for (int i=numOutputs + 1; i<MAX_OUTPUTS; i++) {
      digitalWrite(outputPins[i], LOW);
    }
  }
  // TODO! if numOutputs != oldNumOutputs, blink all the wires numOutput times for easy setting

  // parse FFT data
  if (myFFT.available()) {
    int numOutputBins, nextOutputStartBin, outputId = 0;
    int outputStartBin = FFT_IGNORED_BINS;  // ignore the first FFT_IGNORED_BINS bins as they can be far too noisy. maybe the eq/filters can help tho?

    int expectedBinsPerOutput = numFftBinsWeCareAbout / numOutputs;

    // set the overall sensitivity based on how loud the previous inputs were
    float inputSensitivity = lastLoudestLevel * MINIMUM_INPUT_RANGE;
    if (inputSensitivity < MINIMUM_INPUT_SENSITIVITY) {
      inputSensitivity = MINIMUM_INPUT_SENSITIVITY;
    }
    lastLoudestLevel = 0;

    // DEBUGGING
    Serial.print(numFftBinsWeCareAbout);
    Serial.print(" FFT bins >");
    Serial.print(inputSensitivity, 3);
    Serial.print(" across ");
    Serial.print(numOutputs);
    Serial.print(" outputs (");
    Serial.print(avgInputLevel[MAX_OUTPUTS]);
    Serial.print(" avg): ");
    // END DEBUGGING

    float inputLevelAccumulator = 0;
    while (outputStartBin < numFftBinsWeCareAbout + FFT_IGNORED_BINS) {
      // todo: do we actually want even output sizes? maybe the first bins should be smaller
      if (outputId < numOutputs - 1) {
        nextOutputStartBin = outputStartBin + expectedBinsPerOutput;
        numOutputBins = expectedBinsPerOutput;
      } else {
        // last output
        // make sure we get the last bin in case of rounding errors
        nextOutputStartBin = numFftBinsWeCareAbout + FFT_IGNORED_BINS;
        numOutputBins = nextOutputStartBin - outputStartBin;
      }

      // .read(x, y) gives us a sum of bins x through y so we divide to keep the average
      float outputLevel = myFFT.read(outputStartBin, nextOutputStartBin - 1) / (float)numOutputBins;

      // take an average of the true input
      avgInputLevel[outputId] += EMA_ALPHA * (outputLevel - avgInputLevel[outputId]);   // todo: not sure about this

      if (outputLevel > lastLoudestLevel) {
        lastLoudestLevel = outputLevel;
      }

      if (avgInputLevel[outputId] > inputSensitivity) {
        outputLevel -= avgInputLevel[outputId] * 1.0;  // go numb to loud sounds. todo: tune this
      }
      inputLevelAccumulator += outputLevel;

      /*
      // DEBUGGING
      Serial.print("Reading ");
      Serial.print(outputStartBin);
      Serial.print(" to ");
      Serial.print(nextOutputStartBin - 1);
      Serial.print(" (");
      Serial.print(numOutputBins);
      Serial.println(")");
      */
      printNumber(outputLevel - inputSensitivity, 0);
      // END DEBUGGING

      // turn the light on if outputLevel > inputSensitivity and this isn't a solitary sound. off otherwise
      if (outputLevel > inputSensitivity) {
        if (avgInputLevel[MAX_OUTPUTS] < inputSensitivity) {    // TODO! this is probably wrong
          // we have a loud sound, but the other outputs are quiet, so ignore it
          ;  //Serial.println("Ignored...");
        } else {
          outputStates[outputId] = HIGH;

          // save the time this output turned on. morse code waits for this to be old
          lastOnMs = nowMs;

          // save the time we turned on unless we are in the minimumOnMs window of a previous on
          if (nowMs - lastOnMsArray[outputId] > minimumOnMs) {
            lastOnMsArray[outputId] = lastOnMs;
          }
        }
      } else {
        if (nowMs - lastOnMsArray[outputId] > minimumOnMs) {
          // the output has been on for at least minimumOnMs. turn it off
          // don't actually turn off here because that might break the morse code
          outputStates[outputId] = LOW;
        }
      }

      outputStartBin = nextOutputStartBin;
      outputId += 1;
    }

    Serial.println();

    avgInputLevel[MAX_OUTPUTS] += EMA_ALPHA * (inputLevelAccumulator - avgInputLevel[MAX_OUTPUTS]);   // todo: not sure about this
  }

  if (nowMs - lastOnMs < MAX_OFF_MS) {
    // we turned a light on recently. send output states to the wires
    for (int i=0; i<numOutputs; i++) {
      digitalWrite(outputPins[i], outputStates[i]);
    }
  } else {
    // no lights have been turned on for at least MAX_OFF_MS
    unsigned long checkMs = MAX_OFF_MS;

    bool morse_command_found = false;

    for (int i = 0; i < morse_array_length; i++) {
      MorseCommand morse_command = morse_array[i];
      checkMs = checkMs + morse_command.checkMs;
      if (nowMs < checkMs) {
        bool firstRun = (lastMorseId != i);
        if (firstRun) {
          lastMorseId = i;
          /*
          // DEBUGGING
          Serial.print(nowMs);
          Serial.print(" < ");
          Serial.print(checkMs);
          Serial.print(" - Output: ");
          Serial.print(morseOutputId);
          Serial.print(" - Action #");
          Serial.print(i);
          Serial.print(": ");
          Serial.print(morse_command.outputAction);
          Serial.print(" for ");
          Serial.print(morse_command.checkMs);
          Serial.println();
          // END DEBUGGING
          */

          if (morse_command.outputAction == OUTPUT_ON) {
            digitalWrite(morseOutputId, HIGH);
          } else if (morse_command.outputAction == OUTPUT_OFF) {
            digitalWrite(morseOutputId, LOW);
          } else {
            // make sure the wire is off
            digitalWrite(morseOutputId, LOW);
            // turn on a random wire
            // TODO: should we allow doing random or next?
            int oldMorseOutputId = morseOutputId;
            while (oldMorseOutputId == morseOutputId) {
              morseOutputId = random(numOutputs);
            }

            Serial.print("New morseOutputId: ");
            Serial.println(morseOutputId);
          }
        }

        morse_command_found = true;
        break;
      }
    }

    if (not morse_command_found) {
      // we hit the end of the loop
      // reset time so we start at the beginning of the message
      Serial.println("Loop morse code message...");
      lastOnMs = 0;
      nowMs = MAX_OFF_MS;

      /*
      // reset the lastOnMsArray loop. todo: we might not need this
      for (int i = 0; i < MAX_OUTPUTS; i++) {
        lastOnMsArray[i] = 0;
      }
      */
    }
  }
}
