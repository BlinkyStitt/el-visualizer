// control for Bryan's Jacket with a configurable number of EL wires
//
// when it is quiet, lights flash out morse code BRYAN
// when it is loud, lights flash with the music
//

// you can customize these
#define BUTTON_HOLD_MS 200
#define BUTTON_INTERVAL 20
#define MAX_OUTPUTS 8
#define OUTPUT_A 0
#define OUTPUT_B 1
#define OUTPUT_C 2
#define OUTPUT_D 3
#define OUTPUT_E 4
#define OUTPUT_F 5
#define OUTPUT_G 8
#define OUTPUT_H 20
#define INPUT_NUM_OUTPUTS_KNOB 15
#define INPUT_SENSITIVITY_DOWN 16
#define INPUT_SENSITIVITY_UP 17
#define FFT_AVERAGE_TOGETHER 8  // todo: tune this. higher values = slow refresh rate. default 8 = ~344/8 refreshes per second
#define FFT_IGNORED_BINS 1  // skip the first bin
#define FFT_MAX_BINS 127
#define MAX_OFF_MS 5000
#define MAX_MORSE_ARRAY_LENGTH 256  // todo: tune this
#define DEFAULT_OUTPUTS 6
#define DEFAULT_MORSE_STRING "HELLO WORLD"

const int minimumOnMs = 200;
// http://www.kent-engineers.com/codespeed.htm
const int morseDitMs = 250;
const int morseDahMs = morseDitMs * 3;
const int morseElementSpaceMs = 825;  // 10% longer than a dah
const int morseWordSpaceMs = morseElementSpaceMs * 3;
// END you can customize these

const int outputPins[MAX_OUTPUTS] = {OUTPUT_A, OUTPUT_B, OUTPUT_C, OUTPUT_D, OUTPUT_E, OUTPUT_F, OUTPUT_G, OUTPUT_H};

#define OUTPUT_ON 0
#define OUTPUT_OFF 1
#define OUTPUT_OFF_NEXT_OUTPUT 2

#include <Audio.h>
#include <Bounce2.h>
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
Bounce                   outputSensitivityUpButton = Bounce();
Bounce                   outputSensitivityDownButton = Bounce();

struct MorseCommand {
  unsigned int checkMs;
  unsigned int outputAction;
};
int morse_array_length = MAX_MORSE_ARRAY_LENGTH;
MorseCommand morse_array[MAX_MORSE_ARRAY_LENGTH];
const int numFftBinsWeCareAbout = 24;


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
  audioShield.eqBands(-1.0, -0.10, 0, 0.10, 0.33);  // todo: tune this

  // todo: autoVolume?

  myFFT.averageTogether(FFT_AVERAGE_TOGETHER);

  for (int i=0; i<MAX_OUTPUTS; i++) {
    pinMode(outputPins[i], OUTPUT);
  }

  // setup num_outputs knob
  pinMode(INPUT_NUM_OUTPUTS_KNOB, INPUT);

  // setup output sensitivity buttons
  pinMode(INPUT_SENSITIVITY_DOWN, INPUT_PULLUP);
  outputSensitivityDownButton.attach(INPUT_SENSITIVITY_DOWN);
  outputSensitivityDownButton.interval(BUTTON_INTERVAL);
  pinMode(INPUT_SENSITIVITY_UP, INPUT_PULLUP);
  outputSensitivityUpButton.attach(INPUT_SENSITIVITY_UP);
  outputSensitivityUpButton.interval(BUTTON_INTERVAL);

  // read text off the SD card and translate it into morse code
  SPI.setMOSI(7);
  SPI.setSCK(14);
  if (!(SD.begin(10))) {
    while (1) {
      // todo: allow not having a SD card. just do the default string then
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  // todo: support a directory with multiple text files and pick randomly
  File morseFile = SD.open("morse.txt", FILE_READ);

  String morseString = "";
  if (morseFile) {
    Serial.println("Reading morse.txt...");
    // if the file is available, read it:
    while (morseFile.available()) {
      // todo: this is wrong, but im just testing
      morseString += (char)morseFile.read();
    }
    morseFile.close();
    // TODO! do something to turn the morse code string into something we can easily use to blink
  } else {
    // if the file isn't open, pop up an error:
    Serial.println("Unable to open morse.txt...");
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

  // give things some time to start
  // todo: not sure if this is actually needed
  Serial.println("Starting in 1 second...");
  delay(1000);
}


elapsedMillis nowMs = 0;
int blinkOutput = OUTPUT_A, numOutputs = DEFAULT_OUTPUTS, morseOutputId = random(numOutputs);
float outputSensitivity = 0.050;
bool outputSensitivityUpButtonState, outputSensitivityDownButtonState = false;
unsigned long outputSensitivityUpButtonPressTimeStamp, outputSensitivityDownButtonPressTimeStamp;
unsigned long lastOnMs;
unsigned long lastOnMsArray[MAX_OUTPUTS];
int lastMorseId = 0;


void loop() {
  // configure number of outputs
  // todo: have one pin based on motion sensor

  numOutputs = (int)analogRead(INPUT_NUM_OUTPUTS_KNOB) / (1024 / MAX_OUTPUTS) + 1;

  // todo: too much copypasta in this
  if (outputSensitivityDownButton.update()) {
    if (outputSensitivityDownButton.fell()) {
      Serial.println("down button pressed");
      outputSensitivityDownButtonState = true;
      outputSensitivity = outputSensitivity - 0.001;
      if (outputSensitivity < -0.001) {
        outputSensitivity = -0.001;
      }
    } else {
      outputSensitivityDownButtonState = false;
    }
  } else if (outputSensitivityDownButtonState) {
    if (millis() - outputSensitivityDownButtonPressTimeStamp >= BUTTON_HOLD_MS) {
      Serial.println("Output sensitivity down button held down");
      outputSensitivityDownButtonPressTimeStamp = millis();
      outputSensitivity = outputSensitivity - 0.002;  // todo: ramp this up
      if (outputSensitivity < -0.001) {
        outputSensitivity = -0.001;
      }
    }
  } else if (outputSensitivityUpButton.update()) {
    if (outputSensitivityUpButton.fell()) {
      Serial.println("Output sensitivity up button pressed");
      outputSensitivityUpButtonState = true;
      outputSensitivityUpButtonPressTimeStamp = millis();
      outputSensitivity = outputSensitivity + 0.001;
      if (outputSensitivity > 1.001) {
        outputSensitivity = 1.001;
      }
    } else {
      outputSensitivityUpButtonState = false;
    }
  } else if (outputSensitivityUpButtonState) {
    if (millis() - outputSensitivityUpButtonPressTimeStamp >= BUTTON_HOLD_MS) {
      Serial.println("Output sensitivity up button held down");
      outputSensitivityUpButtonPressTimeStamp = millis();
      outputSensitivity = outputSensitivity + 0.002;    // todo: ramp this up
      if (outputSensitivity > 1.001) {
        outputSensitivity = 1.001;
      }
    }
  }

  if (outputSensitivity < 1 && myFFT.available()) {
    int expectedBinsPerOutput = numFftBinsWeCareAbout / numOutputs;

    int numOutputBins, nextOutputStartBin, outputId = 0;
    int outputStartBin = FFT_IGNORED_BINS;  // ignore bin 0. it is far too noisy. maybe the eq/filters can help tho?

    // DEBUGGING
    Serial.print(numFftBinsWeCareAbout);
    Serial.print(" FFT bins >");
    Serial.print(outputSensitivity, 3);
    Serial.print(" across ");
    Serial.print(numOutputs);
    Serial.print(" outputs (");
    Serial.print(expectedBinsPerOutput);
    Serial.print("):  ");
    // END DEBUGGING

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
      printNumber(outputLevel, outputSensitivity);
      // END DEBUGGING

      // turn the light on if outputLevel > outputSensitivity. off otherwise
      if (outputLevel > outputSensitivity) {
        // todo: save the time that we turned the light on
        digitalWrite(outputPins[outputId], HIGH);

        // save the time this output turned on
        lastOnMs = nowMs;
        lastOnMsArray[outputId] = lastOnMs;
      } else {
        if (nowMs - lastOnMsArray[outputId] > minimumOnMs) {
          // the output has been on for at least minimumOnMs. turn it off
          digitalWrite(outputPins[outputId], LOW);
        }
      }

      outputStartBin = nextOutputStartBin;
      outputId = outputId + 1;
    }

    Serial.println();
  }

  if (nowMs - lastOnMs >= MAX_OFF_MS) {
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
  } else {
    // todo: print this less often.
    //Serial.print("AudioMemoryUsageMax: ");
    //Serial.println(AudioMemoryUsageMax());
  }
}
